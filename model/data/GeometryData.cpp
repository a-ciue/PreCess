#include "GeometryData.h"
#include "GeometryDataVtk.h"

#include <BRep_Builder.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Shape.hxx>

#include <stdexcept>
#include <utility>

namespace {
/**
 * @brief 将形状递归展开并加入目标 Compound，避免直接子节点仍是 Compound。
 */
void addFlattenedShape(
    BRep_Builder& builder,
    TopoDS_Compound& target,
    const TopoDS_Shape& shape)
{
    if (shape.ShapeType() != TopAbs_COMPOUND) {
        builder.Add(target, shape);
        return;
    }

    for (TopoDS_Iterator it(shape); it.More(); it.Next())
        addFlattenedShape(builder, target, it.Value());
}

/**
 * @brief 检查根节点是否为严格一层扁平的 Compound。
 */
bool isFlatRootCompound(const TopoDS_Shape& shape)
{
    if (shape.ShapeType() != TopAbs_COMPOUND)
        return false;

    for (TopoDS_Iterator it(shape); it.More(); it.Next()) {
        if (it.Value().ShapeType() == TopAbs_COMPOUND)
            return false;
    }
    return true;
}

/**
 * @brief 将输入形状规范化为严格一层扁平的根 Compound。
 */
TopoDS_Shape makeFlatRootCompound(const TopoDS_Shape& shape)
{
    if (shape.IsNull())
        throw std::invalid_argument("Geometry root shape is null");

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    addFlattenedShape(builder, compound, shape);
    return compound;
}
}

GeometryData::GeometryData() = default;
GeometryData::~GeometryData() = default;

std::unique_ptr<GeometryData> GeometryData::clone() const
{
    auto copy = std::make_unique<GeometryData>();
    if (rootShape)
        copy->rootShape = std::make_unique<TopoDS_Shape>(*rootShape);
    // gid 向量是身份数据随快照保留（恢复路径 build 时按原值 reclaim）；
    // type_maps 是派生缓存不进快照（保持未建，由恢复路径 ensureIndexBuilt 重建）
    copy->index.vertex_local_to_global = index.vertex_local_to_global;
    copy->index.edge_local_to_global = index.edge_local_to_global;
    copy->index.face_local_to_global = index.face_local_to_global;
    copy->index.solid_local_to_global = index.solid_local_to_global;
    return copy;
}

void GeometryData::setRootShape(TopoDS_Shape shape)
{
    rootShape = std::make_unique<TopoDS_Shape>(
        makeFlatRootCompound(shape));
}

void GeometryData::appendRootShape(TopoDS_Shape shape)
{
    if (shape.IsNull())
        throw std::invalid_argument("Geometry shape is null");

    if (!rootShape || rootShape->IsNull()) {
        setRootShape(std::move(shape));
        return;
    }

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    addFlattenedShape(builder, compound, *rootShape);
    addFlattenedShape(builder, compound, shape);
    rootShape = std::make_unique<TopoDS_Shape>(std::move(compound));
}

std::optional<GeometryDataVtk> GeometryData::getGeometryData()
{
	GeometryDataVtk modelData { *this->rootShape };
	return modelData;
} 

void GeometryData::ensureIndexBuilt(GeometryRegistry& reg)
{
    if (!rootShape || rootShape->IsNull())
        return;
    if (!index.built) {
        // 在模型入口统一兜底，避免插件直接写入裸 Shape 后破坏根 Compound 约束。
        if (!isFlatRootCompound(*rootShape))
            setRootShape(*rootShape);
        index.build(*rootShape, reg);
    }
}