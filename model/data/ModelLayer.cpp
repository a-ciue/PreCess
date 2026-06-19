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
#include "GeometryData.h"
#include "MeshData.h"
#include "ComponentOperator.h"

#include <spdlog/spdlog.h>
#include <filesystem>
#include <stdexcept>

Index ModelLayer::addModel(const std::string& model_name, ComponentDatas components)
{
    Index model_id = ++max_index_;

    auto model = std::make_unique<ModelData>();
    model->model_name_ = model_name;

    for (auto& c : components) {
        if (!c)
            continue;
        c->id = allocateComponentId();
    }

    for (auto& c : components) {
        if (!c)
            continue;
        Index cid = c->id;
        spdlog::info("insert component: final_id={}, exists_before={}",
            cid, components_.count(cid) != 0);

        component_to_model_[cid] = model_id;
        model->componentIds().push_back(cid);

        components_[cid] = std::move(c);
        ComponentData* cp = components_[cid].get();

        if (cp->geometry) {
            cp->geometry->ensureIndexBuilt(geom_registry_);
        }

        if (cp->mesh) {
            MeshData& md = *cp->mesh;
            const Index base = appendGlobalPoints(md.vertex_positions_);
            md.vertex_count_ = (Index)md.vertex_positions_.size();
            md.local_to_global_.resize(md.vertex_count_);
            for (Index i = 0; i < md.vertex_count_; ++i) {
                md.local_to_global_[i] = base + i;
            }
            md.makePointIdsGlobal();
            std::vector<std::array<double, 3>> {}.swap(md.vertex_positions_);
            md.ensureEdgeIdMapBuilt(edge_id_map_, cid);
        }
    }

    models_[model_id] = std::move(model);

    if (observer_)
        observer_->notifyModelAdded(max_index_);
    return model_id;
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
            c->mesh->releaseEdgeIdMap(edge_id_map_);
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

std::optional<ModelOperator> ModelLayer::getModelOperator(Index model_id) const
{
    ModelData* m = modelById(model_id);
    if (m) {
        return ModelOperator(model_id, *m, observer_);
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
    return ComponentOperator(component_id, *c, *this, observer_, model_id);
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

const std::vector<std::array<double, 3>>& ModelLayer::globalPoints() const
{
    return global_points_;
}

MeshIDMap& ModelLayer::edgeIdMap()
{
    return edge_id_map_;
}

const MeshIDMap& ModelLayer::edgeIdMap() const
{
    return edge_id_map_;
}

GeometryRegistry& ModelLayer::geomRegistry()
{
    return geom_registry_;
}

const GeometryRegistry& ModelLayer::geomRegistry() const
{
    return geom_registry_;
}

Index ModelLayer::appendGlobalPoints(const std::vector<std::array<double, 3>>& pts)
{
    Index base = (Index)global_points_.size();
    global_points_.insert(global_points_.end(), pts.begin(), pts.end());
    return base;
}