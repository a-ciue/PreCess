#include "GeometryTopologyEditor.h"

#include <BRepCheck_Analyzer.hxx>
#include <BRep_Builder.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopTools_ShapeMapHasher.hxx>

#include <stdexcept>
#include <vector>

namespace {
// 返回非级联删除时需要保留的直接下级拓扑类型。
TopAbs_ShapeEnum lowerShapeType(TopAbs_ShapeEnum type)
{
    switch (type) {
    case TopAbs_SOLID:
        return TopAbs_FACE;
    case TopAbs_FACE:
        return TopAbs_EDGE;
    case TopAbs_EDGE:
        return TopAbs_VERTEX;
    default:
        return TopAbs_SHAPE;
    }
}

bool isSupportedShapeType(TopAbs_ShapeEnum type)
{
    return type == TopAbs_VERTEX || type == TopAbs_EDGE
        || type == TopAbs_FACE || type == TopAbs_SOLID;
}
}

TopoDS_Shape GeometryTopologyEditor::removeTopLevelShape(
    const TopoDS_Shape& root,
    const TopoDS_Shape& target,
    bool delete_children)
{
    if (root.IsNull() || target.IsNull())
        throw std::invalid_argument("Geometry root and selected shape must not be null");
    if (!isSupportedShapeType(target.ShapeType()))
        throw std::invalid_argument("Only vertex, edge, face or solid can be deleted");

    std::vector<TopoDS_Shape> retained_shapes;
    bool target_found = false;

    // 阶段 1 只修改根形状或扁平根 Compound 的直接子形状，避免隐式破坏 Solid/Shell。
    if (root.IsSame(target)) {
        target_found = true;
    } else if (root.ShapeType() == TopAbs_COMPOUND) {
        for (TopoDS_Iterator it(root); it.More(); it.Next()) {
            const TopoDS_Shape& child = it.Value();
            if (child.IsSame(target)) {
                target_found = true;
                continue;
            }
            retained_shapes.push_back(child);
        }
    }

    if (!target_found)
        throw std::invalid_argument("Only a top-level independent geometry shape can be deleted");

    const TopAbs_ShapeEnum lower_type = lowerShapeType(target.ShapeType());
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> retained_lower_shapes;
    if (lower_type != TopAbs_SHAPE) {
        for (const TopoDS_Shape& shape : retained_shapes)
            TopExp::MapShapes(shape, lower_type, retained_lower_shapes);
    }

    BRep_Builder builder;
    TopoDS_Compound result;
    builder.MakeCompound(result);
    int result_child_count = 0;

    for (const TopoDS_Shape& shape : retained_shapes) {
        builder.Add(result, shape);
        ++result_child_count;
    }

    // 非级联删除时，仅提升没有被其他保留形状引用的直接下级拓扑。
    if (!delete_children && lower_type != TopAbs_SHAPE) {
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> lower_shapes;
        TopExp::MapShapes(target, lower_type, lower_shapes);
        for (int index = 1; index <= lower_shapes.Extent(); ++index) {
            const TopoDS_Shape& lower_shape = lower_shapes.FindKey(index);
            if (retained_lower_shapes.Contains(lower_shape))
                continue;
            builder.Add(result, lower_shape);
            ++result_child_count;
        }
    }

    if (result_child_count == 0)
        return {};
    if (!BRepCheck_Analyzer(result).IsValid())
        throw std::runtime_error("Deleting the geometry shape produced invalid topology");
    return result;
}
