#include "ComponentOperator.h"

#include "ComponentData.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "MeshData.h"
#include "GeometryData.h"
#include "ModelData.h"

#include <BRep_Builder.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Iterator.hxx>
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
        component_->geometry->rootShape = std::make_unique<TopoDS_Shape>(std::move(shape));
        component_->geometry->ensureIndexBuilt(mgr_->geomRegistry());
        notifyChanged();
        return component_id_;
    }

    if (component_->mapping && !component_->mapping->empty())
        throw std::invalid_argument("Target component already contains geometry-mesh mapping");

    const TopoDS_Shape& old_root = *component_->geometry->rootShape;
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);

    // 保持根 Compound 扁平，避免连续创建几何时形成多层嵌套。
    if (old_root.ShapeType() == TopAbs_COMPOUND) {
        for (TopoDS_Iterator it(old_root); it.More(); it.Next())
            builder.Add(compound, it.Value());
    } else {
        builder.Add(compound, old_root);
    }
    builder.Add(compound, shape);

    // 根形状改变后旧业务 ID 不再有效，必须释放并重新建立索引。
    component_->geometry->index.release(mgr_->geomRegistry());
    component_->geometry->rootShape = std::make_unique<TopoDS_Shape>(std::move(compound));
    component_->geometry->ensureIndexBuilt(mgr_->geomRegistry());
    notifyChanged();
    return component_id_;
}