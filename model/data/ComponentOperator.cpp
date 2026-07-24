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

void ComponentOperator::notifyChanged() const
{
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

    component_->mesh->releaseEdgeIdMap(mgr_->edgeIdMap());
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
