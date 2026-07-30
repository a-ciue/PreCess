#include "ComponentOperator.h"

#include "ComponentData.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "MeshData.h"
#include "GeometryData.h"
#include "ModelData.h"

#include <stdexcept>

ComponentOperator::ComponentOperator(Index component_id,
    ComponentData& component,
    ModelLayer& mgr,
    ModelObserver* observer,
    Index model_id) noexcept
    : component_id_(component_id)
    , model_id_(model_id)
    , component_(&component)
    , mgr_(&mgr)
    , observer_(observer)
{
}

MeshData* ComponentOperator::mesh() const noexcept
{
    return component_ && component_->mesh ? component_->mesh.get() : nullptr;
}

GeometryData* ComponentOperator::geometry() const noexcept
{
    return component_ && component_->geometry ? component_->geometry.get() : nullptr;
}

Index ComponentOperator::modelId() const noexcept
{
    return model_id_;
}

ModelData* ComponentOperator::model() const
{
    return mgr_->modelById(model_id_);
}

Index ComponentOperator::materializeEdge(Index p0, Index p1)
{
    MeshData* mesh_data = mesh();
    if (!mesh_data)
        throw std::runtime_error("ComponentOperator::materializeEdge: component has no mesh");
    if (p0 < 0 || p1 < 0 || p0 == p1)
        throw std::invalid_argument("ComponentOperator::materializeEdge: invalid endpoints");

    // 已物化则幂等返回既有 cell 序号
    if (auto row = component_->mesh_adjacency.findEdgeByEndpoints(*mesh_data, p0, p1)) {
        const Index cell = component_->mesh_adjacency.edgeCellIndex(*mesh_data, *row);
        if (cell >= 0)
            return cell;
    }

    const Index cell_index = static_cast<Index>(mesh_data->edge_vertices_.size() / 2);
    mesh_data->edge_vertices_.push_back(p0);
    mesh_data->edge_vertices_.push_back(p1);

    // 同步分配全局边 id
    component_->mesh_adjacency.ensureEdgeGlobalIds(mgr_->edgeIdMap(), component_id_, *mesh_data);

    // 失效邻接索引并通知观察者
    notifyChanged();
    return cell_index;
}

void ComponentOperator::notifyChanged() const
{
    // 网格拓扑可能已变更，派生的邻接索引随通知一并失效
    if (component_)
        component_->mesh_adjacency.invalidate();

    if (!observer_) return;

    observer_->notifyComponentChanged(component_id_);
}