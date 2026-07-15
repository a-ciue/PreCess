#include "GeometryBuilder.h"

#include <BRepCheck_Analyzer.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <gp_Pnt.hxx>

#include <array>
#include <cmath>
#include <stdexcept>

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
