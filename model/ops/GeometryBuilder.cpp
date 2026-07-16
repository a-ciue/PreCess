#include "GeometryBuilder.h"

#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Tool.hxx>
#include <Precision.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Pnt.hxx>

#include <array>
#include <cmath>
#include <stdexcept>

TopoDS_Shape GeometryBuilder::makePoint(double x, double y, double z)
{
    // 独立点同样由 QML 数值输入，进入 OCC 前拒绝空值产生的 NaN。
    const std::array<double, 3> values { x, y, z };
    for (double value : values) {
        if (!std::isfinite(value))
            throw std::invalid_argument("Point coordinates must be finite numbers");
    }

    BRepBuilderAPI_MakeVertex builder(gp_Pnt(x, y, z));
    if (!builder.IsDone())
        throw std::runtime_error("OpenCASCADE failed to create the point");

    TopoDS_Shape shape = builder.Shape();
    if (shape.IsNull())
        throw std::runtime_error("OpenCASCADE returned an empty point");

    if (!BRepCheck_Analyzer(shape).IsValid())
        throw std::runtime_error("The created point is topologically invalid");

    return shape;
}

namespace {
// 统一检查直线边构造结果，避免两个入口重复错误处理。
TopoDS_Shape checkedLine(BRepBuilderAPI_MakeEdge& builder)
{
    if (!builder.IsDone())
        throw std::runtime_error("OpenCASCADE failed to create the line");

    TopoDS_Shape shape = builder.Shape();
    if (shape.IsNull())
        throw std::runtime_error("OpenCASCADE returned an empty line");
    if (!BRepCheck_Analyzer(shape).IsValid())
        throw std::runtime_error("The created line is topologically invalid");
    return shape;
}
}

TopoDS_Shape GeometryBuilder::makeLine(
    double start_x,
    double start_y,
    double start_z,
    double end_x,
    double end_y,
    double end_z)
{
    const std::array<double, 6> values {
        start_x, start_y, start_z, end_x, end_y, end_z
    };
    for (double value : values) {
        if (!std::isfinite(value))
            throw std::invalid_argument("Line coordinates must be finite numbers");
    }

    const gp_Pnt start(start_x, start_y, start_z);
    const gp_Pnt end(end_x, end_y, end_z);
    if (start.Distance(end) <= Precision::Confusion())
        throw std::invalid_argument("Line endpoints must be different");

    BRepBuilderAPI_MakeEdge builder(start, end);
    return checkedLine(builder);
}

TopoDS_Shape GeometryBuilder::makeLine(
    const TopoDS_Vertex& start,
    const TopoDS_Vertex& end)
{
    if (start.IsNull() || end.IsNull())
        throw std::invalid_argument("Line vertices must not be null");
    if (start.IsSame(end)
        || BRep_Tool::Pnt(start).Distance(BRep_Tool::Pnt(end)) <= Precision::Confusion())
        throw std::invalid_argument("Line endpoints must be different");

    // 使用已有 TopoDS_Vertex，保证新 Edge 与输入点共享拓扑。
    BRepBuilderAPI_MakeEdge builder(start, end);
    return checkedLine(builder);
}

TopoDS_Shape GeometryBuilder::makeBox(
    double origin_x,
    double origin_y,
    double origin_z,
    double length_x,
    double length_y,
    double length_z)
{
    // QML 空输入会产生 NaN，进入 OCC 前统一拒绝非有限数值。
    const std::array<double, 6> values {
        origin_x, origin_y, origin_z, length_x, length_y, length_z
    };
    for (double value : values) {
        if (!std::isfinite(value))
            throw std::invalid_argument("Box parameters must be finite numbers");
    }

    if (length_x <= 0.0 || length_y <= 0.0 || length_z <= 0.0)
        throw std::invalid_argument("Box dimensions must be greater than zero");

    // 先由 OCC 创建实体，再依次检查构造状态、空 Shape 和拓扑合法性。
    BRepPrimAPI_MakeBox builder(
        gp_Pnt(origin_x, origin_y, origin_z),
        length_x,
        length_y,
        length_z);
    builder.Build();
    if (!builder.IsDone())
        throw std::runtime_error("OpenCASCADE failed to create the box");

    TopoDS_Shape shape = builder.Shape();
    if (shape.IsNull())
        throw std::runtime_error("OpenCASCADE returned an empty box");

    if (!BRepCheck_Analyzer(shape).IsValid())
        throw std::runtime_error("The created box is topologically invalid");

    return shape;
}
