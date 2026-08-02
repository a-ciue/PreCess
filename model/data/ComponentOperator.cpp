#include "ComponentOperator.h"

#include "ComponentData.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "MeshData.h"
#include "GeometryData.h"
#include "ModelData.h"

#include <TopoDS_Shape.hxx>

#include <stdexcept>
#include <utility>

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
    if (auto edge = component_->mesh_adjacency.findEdgeByEndpoints(*mesh_data, p0, p1)) {
        const Index cell = component_->mesh_adjacency.edgeCellIndex(*mesh_data, *edge);
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

Index ComponentOperator::appendGeometryShape(TopoDS_Shape shape)
{
    if (shape.IsNull())
        throw std::invalid_argument("Geometry shape is null");

    // 当前组件尚无有效几何时，直接用新形状初始化，不额外创建 Component。
    if (!component_->geometry)
        component_->geometry = std::make_unique<GeometryData>();
    if (!component_->geometry->rootShape || component_->geometry->rootShape->IsNull()) {
        if (component_->geometry->index.built)
            component_->geometry->index.release(mgr_->geomRegistry());
        component_->geometry->setRootShape(std::move(shape));
        component_->geometry->ensureIndexBuilt(mgr_->geomRegistry());
        notifyChanged();
        return component_id_;
    }

    if (component_->mapping && !component_->mapping->empty())
        throw std::invalid_argument("Target component already contains geometry-mesh mapping");

    // 根形状改变后旧业务 ID 不再有效，必须释放并重新建立索引。
    component_->geometry->index.release(mgr_->geomRegistry());
    component_->geometry->appendRootShape(std::move(shape));
    component_->geometry->ensureIndexBuilt(mgr_->geomRegistry());
    notifyChanged();
    return component_id_;
}

Index ComponentOperator::replaceGeometryRoot(TopoDS_Shape shape)
{
    if (!component_ || !component_->geometry || !component_->geometry->rootShape)
        throw std::invalid_argument("Target component has no geometry");
    if (component_->mapping && !component_->mapping->empty())
        throw std::invalid_argument("Target component already contains geometry-mesh mapping");

    // 根形状变化会使原有业务 ID 失效，先释放旧索引再写入新拓扑。
    component_->geometry->index.release(mgr_->geomRegistry());
    if (shape.IsNull()) {
        component_->geometry.reset();
    } else {
        // 复用几何创建入口，统一维持严格一层扁平的根 Compound 约束。
        component_->geometry->setRootShape(std::move(shape));
        component_->geometry->ensureIndexBuilt(mgr_->geomRegistry());
    }

    notifyChanged();
    return component_id_;
}

void ComponentOperator::removeMesh()
{
    if (!component_ || !component_->mesh)
        return;

    component_->releasePointGlobalIds(mgr_->pointIdMap());
    component_->mesh_adjacency.releaseEdgeGlobalIds(mgr_->edgeIdMap());
    component_->mesh.reset();

    if (observer_)
        observer_->notifyComponentChanged(component_id_);
}

void ComponentOperator::removeGeometry()
{
    if (!component_ || !component_->geometry)
        return;

    component_->geometry->index.release(mgr_->geomRegistry());
    component_->geometry.reset();

    if (observer_)
        observer_->notifyComponentChanged(component_id_);
}
