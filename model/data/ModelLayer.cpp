/**
 * @file ModelLayer.cpp
 * @brief 实现 ModelLayer 类，用于管理多个网格模型
 *
 * 该文件包含 ModelLayer 类的实现，提供多模型管理功能，包括：
 * - 添加、删除和获取模型
 * - 维护与 VTK 组件的交互
 *
 * @author 徐昊阳 haoyangxu06@gmail.com
 * @date 2025/3/20
 */
#include "ModelLayer.h"
#include "ComponentOperator.h"
#include "GeometryData.h"
#include "MeshData.h"
#include "ModelObserver.h"
#include "ModelSnapshot.h"
#include "UndoRecorder.h"

#include <algorithm>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>

Index ModelLayer::addModel(const std::string& model_name, ComponentDatas components)
{
    assertWriteAllowed();
    Index model_id = ++max_index_;

    auto model = std::make_unique<ModelData>();
    model->model_name_ = model_name;
    models_[model_id] = std::move(model);

    // 发号 + adoptComponent：组件 gid 尚未分配，adoptComponent 内 reclaim 自然无操作、ensure 补缺
    for (auto& c : components) {
        if (!c)
            continue;
        adoptComponent(allocateComponentId(), std::move(c), model_id);
    }

    // 先登记结构变化，再发布观察通知，重入回调不会颠倒记录顺序。
    if (undo_recorder_)
        undo_recorder_->onModelAdded(model_id);
    if (observer_)
        observer_->notifyModelAdded(model_id);
    return model_id;
}

std::unique_ptr<ModelSnapshot> ModelLayer::takeModelSnapshot(Index model_id) const
{
    const ModelData* model = modelById(model_id);
    if (!model)
        throw std::runtime_error("Model not exist");

    auto snapshot = std::make_unique<ModelSnapshot>();
    snapshot->model_id = model_id;
    snapshot->name = model->model_name_;
    snapshot->components.reserve(model->componentIds().size());
    for (Index cid : model->componentIds()) {
        if (const ComponentData* c = findComponent(cid))
            snapshot->components.push_back(c->clone());
    }
    return snapshot;
}

Index ModelLayer::restoreModel(const ModelSnapshot& snapshot)
{
    assertWriteAllowed();
    if (snapshot.model_id < 0)
        throw std::invalid_argument("ModelLayer::restoreModel: invalid model id");
    if (models_.count(snapshot.model_id) != 0)
        throw std::runtime_error("ModelLayer::restoreModel: model id occupied");

    // 按原 id 插回；发号器 max_index_ 保持只增不回滚（原 id 必然小于当前水位）
    const Index model_id = snapshot.model_id;
    auto model = std::make_unique<ModelData>();
    model->model_name_ = snapshot.name;
    models_[model_id] = std::move(model);

    // 逐组件按原 id adopt（快照为 const，clone 出副本入池；component_ids_ 顺序随快照还原）
    for (const auto& c : snapshot.components) {
        if (!c)
            continue;
        adoptComponent(c->id, c->clone(), model_id);
    }

    if (observer_)
        observer_->notifyModelAdded(model_id);
    return model_id;
}

void ModelLayer::restoreComponent(Index model_id, std::unique_ptr<ComponentData> component)
{
    assertWriteAllowed();
    if (!component)
        throw std::invalid_argument("ModelLayer::restoreComponent: null component");

    const Index component_id = component->id;
    adoptComponent(component_id, std::move(component), model_id);

    // 与 addGeometryComponent 的通知一致：只新增组件，通知渲染层按组件加载
    if (observer_)
        observer_->notifyComponentChanged(component_id);
}

void ModelLayer::adoptComponent(Index component_id, std::unique_ptr<ComponentData> component, Index model_id)
{
    auto mit = models_.find(model_id);
    if (mit == models_.end() || !mit->second)
        throw std::runtime_error("ModelLayer::adoptComponent: model not exist");
    if (components_.count(component_id) != 0)
        throw std::runtime_error("ModelLayer::adoptComponent: component id occupied");
    assertWriteAllowed();

    spdlog::info("insert component: final_id={}, exists_before={}",
        component_id, components_.count(component_id) != 0);

    component->id = component_id;
    component_to_model_[component_id] = model_id;
    mit->second->componentIds().push_back(component_id);

    ComponentData* cp = component.get();
    components_[component_id] = std::move(component);

    if (cp->geometry) {
        cp->geometry->ensureIndexBuilt(geom_registry_);
    }

    if (cp->mesh) {
        MeshData& md = *cp->mesh;
        md.vertex_count_ = (Index)md.vertex_positions_.size();
        // 先按原值 reclaim 已有 gid（快照恢复），再 ensure 补缺（新入池），两者幂等兼容
        cp->reclaimPointGlobalIds(point_id_map_);
        cp->ensurePointGlobalIds(point_id_map_);
        cp->mesh_adjacency.reclaimEdgeGlobalIds(edge_id_map_, component_id);
        cp->mesh_adjacency.ensureEdgeGlobalIds(edge_id_map_, component_id, md);
    }
}

void ModelLayer::removeModel(Index model_id)
{
    assertWriteAllowed();
    auto it = models_.find(model_id);
    if (it == models_.end())
        throw std::runtime_error("Model not exist");

    // undo 记录钩子：移除前回调（捕获整模型快照）
    if (undo_recorder_)
        undo_recorder_->onModelRemoving(*it->second);

    // 组件随模型快照整体记录：移除期间抑制组件级移除钩子，避免重复记录
    component_remove_hook_suspended_ = true;
    try {
        std::vector<Index> comp_ids = it->second ? it->second->componentIds() : std::vector<Index> { };
        for (Index cid : comp_ids)
            removeComponent(cid);
    } catch (...) {
        component_remove_hook_suspended_ = false;
        throw;
    }
    component_remove_hook_suspended_ = false;

    models_.erase(it);
    if (observer_)
        observer_->notifyModelRemoved(model_id);
}

void ModelLayer::removeComponent(Index component_id)
{
    assertWriteAllowed();
    auto modelIt = component_to_model_.find(component_id);
    if (modelIt == component_to_model_.end())
        throw std::runtime_error("ComponentData not exist");

    // undo 记录钩子：移除前回调（克隆组件快照）；removeModel 期间抑制（随模型快照整体记录）
    if (undo_recorder_ && !component_remove_hook_suspended_) {
        if (ComponentData* c = findComponent(component_id))
            undo_recorder_->onComponentRemoving(*c);
    }

    Index model_id = modelIt->second;

    auto mit = models_.find(model_id);
    if (mit == models_.end() || !mit->second)
        throw std::runtime_error("Owner model not exist");

    auto& ids = mit->second->componentIds();
    ids.erase(std::remove(ids.begin(), ids.end(), component_id), ids.end());

    if (ComponentData* c = findComponent(component_id)) {
        if (c->mesh) {
            c->releasePointGlobalIds(point_id_map_);
            c->mesh_adjacency.releaseEdgeGlobalIds(edge_id_map_);
        }
        if (c->geometry) {
            c->geometry->index.release(geom_registry_);
        }
    }
    components_.erase(component_id);

    component_to_model_.erase(component_id);

    if (observer_)
        observer_->notifyComponentRemoved(component_id);
}

std::optional<ModelOperator> ModelLayer::getModelOperator(Index model_id)
{
    ModelData* m = modelById(model_id);
    if (m) {
        return ModelOperator(model_id, *m, *this);
    }
    return { };
}

ModelData* ModelLayer::modelById(Index model_id)
{
    return const_cast<ModelData*>(std::as_const(*this).modelById(model_id));
}

const ModelData* ModelLayer::modelById(Index model_id) const
{
    auto it = models_.find(model_id);
    if (it == models_.end())
        return nullptr;
    return it->second.get();
}

Index ModelLayer::findModelId(const ModelData& model) const
{
    for (const auto& [id, m] : models_) {
        if (m.get() == &model)
            return id;
    }
    return -1;
}

std::optional<ComponentOperator> ModelLayer::getComponentOperator(Index component_id)
{
    ComponentData* c = findComponent(component_id);
    if (!c)
        return std::nullopt;

    auto mit = component_to_model_.find(component_id);
    Index model_id = mit != component_to_model_.end() ? mit->second : -1;
    return ComponentOperator(component_id, *c, *this, model_id);
}

Index ModelLayer::allocateComponentId() noexcept
{
    return next_component_id_++;
}

ComponentData* ModelLayer::findComponent(Index component_id)
{
    return const_cast<ComponentData*>(std::as_const(*this).findComponent(component_id));
}

const ComponentData* ModelLayer::findComponent(Index component_id) const
{
    auto it = components_.find(component_id);
    return it == components_.end() ? nullptr : it->second.get();
}

std::optional<Index> ModelLayer::findComponentIdByGeometryShapeId(
    TopAbs_ShapeEnum shape_type,
    Index shape_id) const
{
    if (shape_id < 0)
        return std::nullopt;

    // 四类几何 ID 独立编号，必须根据形状类型选择对应索引。
    for (const auto& [component_id, component] : components_) {
        if (!component || !component->geometry)
            continue;

        const std::vector<Index>* shape_ids = nullptr;
        switch (shape_type) {
        case TopAbs_VERTEX:
            shape_ids = &component->geometry->index.vertex_local_to_global;
            break;
        case TopAbs_EDGE:
            shape_ids = &component->geometry->index.edge_local_to_global;
            break;
        case TopAbs_FACE:
            shape_ids = &component->geometry->index.face_local_to_global;
            break;
        case TopAbs_SOLID:
            shape_ids = &component->geometry->index.solid_local_to_global;
            break;
        default:
            return std::nullopt;
        }

        if (std::find(shape_ids->begin(), shape_ids->end(), shape_id)
            != shape_ids->end())
            return component_id;
    }

    return std::nullopt;
}

MeshIDMap& ModelLayer::pointIdMap()
{
    return point_id_map_;
}

const MeshIDMap& ModelLayer::pointIdMap() const
{
    return point_id_map_;
}

MeshIDMap& ModelLayer::edgeIdMap()
{
    return edge_id_map_;
}

const MeshIDMap& ModelLayer::edgeIdMap() const
{
    return edge_id_map_;
}

ModelLayer::ModelLayer(ModelLayer&& other) noexcept
    : geom_registry_(std::move(other.geom_registry_))
    , models_(std::move(other.models_))
    , components_(std::move(other.components_))
    , component_to_model_(std::move(other.component_to_model_))
    , max_index_(other.max_index_)
    , next_component_id_(other.next_component_id_)
    , point_id_map_(std::move(other.point_id_map_))
    , edge_id_map_(std::move(other.edge_id_map_))
    , observer_(other.observer_)
    , pending_notify_(std::move(other.pending_notify_))
    , undo_recorder_(other.undo_recorder_)
    , component_remove_hook_suspended_(other.component_remove_hook_suspended_)
    , write_authority_(other.write_authority_) // 共享同一份写权限状态：移动只搬数据，权限不分裂
{
}

ModelLayer::WriteOperation::WriteOperation(std::shared_ptr<WriteAuthority> authority, std::uint64_t id)
    : authority_(std::move(authority))
    , id_(id)
{
}

ModelLayer::WriteOperation::~WriteOperation() { release(); }

void ModelLayer::WriteOperation::release() noexcept
{
    if (authority_->operation == id_)
        authority_->operation = 0;
}

bool ModelLayer::WriteOperation::active() const noexcept
{
    return authority_->operation == id_;
}

std::unique_ptr<ModelLayer::WriteOperation> ModelLayer::beginWriteOperation(bool masked)
{
    if (std::this_thread::get_id() != write_authority_->owner_thread)
        throw std::runtime_error("ModelLayer: operation must begin on the model owner thread");
    if (write_authority_->operation != 0)
        return nullptr;
    const auto id = ++write_authority_->next_operation;
    // 分配成功后才登记占用；异常不会留下无法释放的操作。
    auto operation = std::make_unique<WriteOperation>(write_authority_, id);
    write_authority_->operation = id;
    write_authority_->masked = masked;
    return operation;
}

ModelLayer::WritePrivilege::WritePrivilege(ModelLayer& layer, const WriteOperation* operation)
    : layer_(&layer)
{
    auto& authority = *layer.write_authority_;
    if (!authority.offthread_ok && std::this_thread::get_id() != authority.owner_thread)
        throw std::runtime_error("ModelLayer: off-thread real-model write rejected (undo records are bound to the GUI thread)");
    if (operation) {
        if (operation->authority_ != layer.write_authority_ || !operation->active())
            throw std::runtime_error("ModelLayer: stale or foreign operation write authority rejected");
    } else if (layer.writesPending() && !layer.hasWritePrivilege()) {
        throw ModelOperationBusy("ModelLayer: operation write authority required while a job's writeback is pending");
    }
    ++authority.privilege;
}

ModelLayer::WritePrivilege::~WritePrivilege()
{
    --layer_->write_authority_->privilege;
}

bool ModelLayer::hasWritePrivilege() const noexcept
{
    return std::this_thread::get_id() == write_authority_->owner_thread
        && write_authority_->privilege > 0;
}

void ModelLayer::assertOwnerThread() const
{
    if (!write_authority_->offthread_ok && std::this_thread::get_id() != write_authority_->owner_thread)
        throw std::runtime_error("ModelLayer: operation requires the model owner thread");
}

void ModelLayer::assertOperationIdle(std::source_location loc) const
{
    assertOwnerThread();
    if (writesPending())
        throw ModelOperationBusy("ModelLayer: operation requires an idle model (from "
            + std::string(loc.file_name()) + ":" + std::to_string(loc.line()) + ")");
}

void ModelLayer::assertWriteAllowed(std::source_location loc) const
{
    const WriteAuthority& authority = *write_authority_;
    if (!authority.offthread_ok && std::this_thread::get_id() != authority.owner_thread)
        throw std::runtime_error("ModelLayer: off-thread real-model write rejected (undo records are bound to the GUI thread)");
    if (writesPending() && !hasWritePrivilege())
        throw ModelOperationBusy("ModelLayer: write rejected while a job's writeback is pending (frozen operation)");
    // 归属由记录器裁决；组件与结构写在同一入口、动数据前拒绝。
    if (undo_recorder_ && !undo_recorder_->allowsUnrecordedWrite()) {
        throw ModelOperationBusy(
            "ModelLayer: unrecorded boundary-less write rejected (from "
            + std::string(loc.file_name()) + ":" + std::to_string(loc.line())
            + ") — open an operation boundary or a scope session first");
    }
}

void ModelLayer::markComponentDirty(Index component_id, MeshEditKind kind, std::source_location loc)
{
    ComponentData* c = findComponent(component_id);
    if (!c)
        return;
    assertWriteAllowed(loc); // 写前标脏契约保证拒绝先于数据、缓存与记录变化。

    // Topology 类修改立即失效邻接懒表，保证查询即时正确；通知延迟到操作边界 flush
    if (kind == MeshEditKind::Topology)
        c->mesh_adjacency.invalidate();

    // undo 记录钩子：写前回调（数据尚未修改），操作边界内首次标脏捕获 before-image。
    if (undo_recorder_)
        undo_recorder_->onComponentDirty(component_id, *c);

    // 去重记入待通知集合
    if (std::find(pending_notify_.begin(), pending_notify_.end(), component_id) == pending_notify_.end())
        pending_notify_.push_back(component_id);
}

void ModelLayer::flushNotifications()
{
    if (pending_notify_.empty())
        return;

    // 通知是对已记账状态的观察；同步监听者不能继承提交授权，在记账后插入新模型写。
    struct PausePrivilege {
        WriteAuthority& authority;
        int depth;
        ~PausePrivilege() { authority.privilege = depth; }
    } pause { *write_authority_, std::exchange(write_authority_->privilege, 0) };

    // 先交换取出并清空再通知：通知链会经 observer → Qt 信号 → EventBus ModelEvent →
    // 功能事件回调（FeatureEventGateway 包装）重入本函数，重入时集合已空即空转，
    // 防止"遍历未清空 → 重入 flush → 再通知"的无限递归。
    // 通知期间产生的新标脏记入 pending_notify_，由下一个操作边界 flush 发出。
    std::vector<Index> pending;
    pending.swap(pending_notify_);
    if (observer_) {
        for (Index component_id : pending)
            observer_->notifyComponentChanged(component_id);
    }
}

GeometryRegistry& ModelLayer::geomRegistry()
{
    return geom_registry_;
}

const GeometryRegistry& ModelLayer::geomRegistry() const
{
    return geom_registry_;
}
