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
#include "ModelObserver.h"
#include "ModelSnapshot.h"
#include "GeometryData.h"
#include "MeshData.h"
#include "ComponentOperator.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <spdlog/spdlog.h>

Index ModelLayer::addModel(const std::string& model_name, ComponentDatas components)
{
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

    if (observer_)
        observer_->notifyModelAdded(model_id);
    return model_id;
}

std::unique_ptr<ModelSnapshot> ModelLayer::takeModelSnapshot(Index model_id) const
{
    ModelData* model = modelById(model_id);
    if (!model)
        throw std::runtime_error("Model not exist");

    auto snapshot = std::make_unique<ModelSnapshot>();
    snapshot->model_id = model_id;
    snapshot->name = model->model_name_;
    snapshot->components.reserve(model->componentIds().size());
    for (Index cid : model->componentIds()) {
        if (ComponentData* c = findComponent(cid))
            snapshot->components.push_back(c->clone());
    }
    return snapshot;
}

Index ModelLayer::restoreModel(const ModelSnapshot& snapshot)
{
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

void ModelLayer::removeModel(Index model_id) {
    auto it = models_.find(model_id);
    if (it == models_.end())
        throw std::runtime_error("Model not exist");

    std::vector<Index> comp_ids = it->second ? it->second->componentIds() : std::vector<Index> {};
    for (Index cid : comp_ids)
        removeComponent(cid);

    models_.erase(it);
    if (observer_)
        observer_->notifyModelRemoved(model_id);
}

void ModelLayer::removeComponent(Index component_id)
{
    auto modelIt = component_to_model_.find(component_id);
    if (modelIt == component_to_model_.end())
        throw std::runtime_error("ComponentData not exist");

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
    return {};
}

ModelData* ModelLayer::modelById(Index model_id) const
{
    auto it = models_.find(model_id);
    if (it == models_.end())
        return nullptr;
    return it->second.get();
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

ComponentData* ModelLayer::findComponent(Index component_id) const
{
    auto it = components_.find(component_id);
    return it == components_.end() ? nullptr : it->second.get();
}

std::optional<Index> ModelLayer::findComponentIdByGeometryFaceId(GeomFaceId face_id) const
{
    if (face_id == kInvalidGeomFaceId)
        return std::nullopt;

    // 全局面 ID 保存在各 Component 的几何索引中，找到后即可确定所有者。
    for (const auto& [component_id, component] : components_) {
        if (!component || !component->geometry)
            continue;

        const auto& face_ids = component->geometry->index.face_local_to_global;
        if (std::find(face_ids.begin(), face_ids.end(), face_id) != face_ids.end())
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

void ModelLayer::markComponentDirty(Index component_id, MeshEditKind kind)
{
    ComponentData* c = findComponent(component_id);
    if (!c)
        return;

    // Topology 类修改立即失效邻接懒表，保证查询即时正确；通知延迟到操作边界 flush
    if (kind == MeshEditKind::Topology)
        c->mesh_adjacency.invalidate();

    // 去重记入待通知集合
    if (std::find(pending_notify_.begin(), pending_notify_.end(), component_id) == pending_notify_.end())
        pending_notify_.push_back(component_id);
}

void ModelLayer::flushNotifications()
{
    if (pending_notify_.empty())
        return;

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
