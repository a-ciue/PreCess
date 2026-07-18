#include "GeometryBuilder.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_WireError.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFill_Filling.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRep_Tool.hxx>
#include <BRepTools_WireExplorer.hxx>
#include <GeomAbs_Shape.hxx>
#include <Precision.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <NCollection_List.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

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

TopoDS_Shape GeometryBuilder::makeRectangleFace(
    double origin_x,
    double origin_y,
    double origin_z,
    double width,
    double height,
    CoordinatePlane plane)
{
    const std::array<double, 5> values {
        origin_x, origin_y, origin_z, width, height
    };
    for (double value : values) {
        if (!std::isfinite(value))
            throw std::invalid_argument("Rectangle face parameters must be finite numbers");
    }
    if (width <= Precision::Confusion() || height <= Precision::Confusion())
        throw std::invalid_argument("Rectangle face dimensions must be greater than tolerance");

    // 原点是矩形角点，宽度和高度分别沿所选平面的两个正轴方向展开。
    std::array<gp_Pnt, 4> points;
    switch (plane) {
    case CoordinatePlane::XY:
        points = { gp_Pnt(origin_x, origin_y, origin_z),
            gp_Pnt(origin_x + width, origin_y, origin_z),
            gp_Pnt(origin_x + width, origin_y + height, origin_z),
            gp_Pnt(origin_x, origin_y + height, origin_z) };
        break;
    case CoordinatePlane::YZ:
        points = { gp_Pnt(origin_x, origin_y, origin_z),
            gp_Pnt(origin_x, origin_y + width, origin_z),
            gp_Pnt(origin_x, origin_y + width, origin_z + height),
            gp_Pnt(origin_x, origin_y, origin_z + height) };
        break;
    case CoordinatePlane::XZ:
        points = { gp_Pnt(origin_x, origin_y, origin_z),
            gp_Pnt(origin_x + width, origin_y, origin_z),
            gp_Pnt(origin_x + width, origin_y, origin_z + height),
            gp_Pnt(origin_x, origin_y, origin_z + height) };
        break;
    default:
        throw std::invalid_argument("Rectangle face plane is invalid");
    }

    // 显式复用四个拓扑点，保证相邻 Edge 共享 Vertex，再依次组成闭合 Wire。
    std::array<TopoDS_Vertex, 4> vertices;
    for (size_t i = 0; i < vertices.size(); ++i) {
        BRepBuilderAPI_MakeVertex vertex_builder(points[i]);
        if (!vertex_builder.IsDone())
            throw std::runtime_error("OpenCASCADE failed to create a rectangle vertex");
        vertices[i] = vertex_builder.Vertex();
    }

    BRepBuilderAPI_MakeWire wire_builder;
    for (size_t i = 0; i < vertices.size(); ++i) {
        BRepBuilderAPI_MakeEdge edge_builder(vertices[i], vertices[(i + 1) % vertices.size()]);
        if (!edge_builder.IsDone())
            throw std::runtime_error("OpenCASCADE failed to create a rectangle edge");
        wire_builder.Add(edge_builder.Edge());
    }
    if (!wire_builder.IsDone())
        throw std::runtime_error("OpenCASCADE failed to create the rectangle wire");

    // OnlyPlane=true：只接受能够识别为平面的闭合 Wire。
    const TopoDS_Wire wire = wire_builder.Wire();
    BRepBuilderAPI_MakeFace face_builder(wire, true);
    if (!face_builder.IsDone())
        throw std::runtime_error("OpenCASCADE failed to create the rectangle face");

    TopoDS_Shape shape = face_builder.Shape();
    if (shape.IsNull())
        throw std::runtime_error("OpenCASCADE returned an empty rectangle face");
    if (!BRepCheck_Analyzer(shape).IsValid())
        throw std::runtime_error("The created rectangle face is topologically invalid");
    return shape;
}

TopoDS_Shape GeometryBuilder::makeDiskFace(
    double center_x,
    double center_y,
    double center_z,
    double radius,
    CoordinatePlane plane)
{
    const std::array<double, 4> values {
        center_x, center_y, center_z, radius
    };
    for (double value : values) {
        if (!std::isfinite(value))
            throw std::invalid_argument("Disk face parameters must be finite numbers");
    }
    if (radius <= Precision::Confusion())
        throw std::invalid_argument("Disk face radius must be greater than tolerance");

    // 为三个全局坐标平面设置法向和局部 X 参考方向。
    const gp_Pnt center(center_x, center_y, center_z);
    gp_Ax2 placement;
    switch (plane) {
    case CoordinatePlane::XY:
        placement = gp_Ax2(center, gp_Dir(0.0, 0.0, 1.0), gp_Dir(1.0, 0.0, 0.0));
        break;
    case CoordinatePlane::YZ:
        placement = gp_Ax2(center, gp_Dir(1.0, 0.0, 0.0), gp_Dir(0.0, 1.0, 0.0));
        break;
    case CoordinatePlane::XZ:
        placement = gp_Ax2(center, gp_Dir(0.0, -1.0, 0.0), gp_Dir(1.0, 0.0, 0.0));
        break;
    default:
        throw std::invalid_argument("Disk face plane is invalid");
    }

    // 圆曲线先生成闭合 Edge，再组成 Wire 并限定创建平面 Face。
    BRepBuilderAPI_MakeEdge edge_builder(gp_Circ(placement, radius));
    if (!edge_builder.IsDone())
        throw std::runtime_error("OpenCASCADE failed to create the disk edge");

    BRepBuilderAPI_MakeWire wire_builder;
    wire_builder.Add(edge_builder.Edge());
    if (!wire_builder.IsDone())
        throw std::runtime_error("OpenCASCADE failed to create the disk wire");

    BRepBuilderAPI_MakeFace face_builder(wire_builder.Wire(), true);
    if (!face_builder.IsDone())
        throw std::runtime_error("OpenCASCADE failed to create the disk face");

    TopoDS_Shape shape = face_builder.Shape();
    if (shape.IsNull())
        throw std::runtime_error("OpenCASCADE returned an empty disk face");
    if (!BRepCheck_Analyzer(shape).IsValid())
        throw std::runtime_error("The created disk face is topologically invalid");
    return shape;
}

TopoDS_Shape GeometryBuilder::makeFaceFromEdges(
    const std::vector<TopoDS_Edge>& edges)
{
    if (edges.empty())
        throw std::invalid_argument("At least one geometry edge is required");

    // 批量 Add 会按连接关系组织无序边；必须检查状态，避免使用断边形成的部分 Wire。
    NCollection_List<TopoDS_Shape> edge_shapes;
    for (const TopoDS_Edge& edge : edges) {
        if (edge.IsNull())
            throw std::invalid_argument("Selected geometry edges must not be null");
        edge_shapes.Append(edge);
    }

    BRepBuilderAPI_MakeWire wire_builder;
    wire_builder.Add(edge_shapes);
    if (!wire_builder.IsDone()
        || wire_builder.Error() != BRepBuilderAPI_WireDone)
        throw std::invalid_argument("Selected geometry edges must form one connected wire");

    const TopoDS_Wire wire = wire_builder.Wire();
    if (!BRep_Tool::IsClosed(wire))
        throw std::invalid_argument("Selected geometry edges must form a closed wire");

    // 共面时优先创建精确平面，保持现有矩形和圆形轮廓的结果不变。
    BRepBuilderAPI_MakeFace planar_builder(wire, true);
    TopoDS_Shape shape;
    if (planar_builder.IsDone()) {
        shape = planar_builder.Shape();
    } else {
        // Filling 要求边界连续输入，因此从已组织好的 Wire 中按连接顺序读取边。
        BRepFill_Filling filling_builder;
        for (BRepTools_WireExplorer exp(wire); exp.More(); exp.Next())
            filling_builder.Add(TopoDS::Edge(exp.Current()), GeomAbs_C0, true);
        filling_builder.Build();
        if (!filling_builder.IsDone())
            throw std::runtime_error("OpenCASCADE failed to fill the non-planar wire");
        shape = filling_builder.Face();
    }

    if (shape.IsNull() || shape.ShapeType() != TopAbs_FACE)
        throw std::runtime_error("OpenCASCADE failed to create the face from edges");
    if (!BRepCheck_Analyzer(shape).IsValid())
        throw std::runtime_error("The created face is topologically invalid");
    return shape;
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

TopoDS_Shape GeometryBuilder::makeCylinder(
    double center_x,
    double center_y,
    double center_z,
    double radius,
    double height,
    double direction_x,
    double direction_y,
    double direction_z)
{
    const std::array<double, 8> values {
        center_x,
        center_y,
        center_z,
        radius,
        height,
        direction_x,
        direction_y,
        direction_z
    };
    for (double value : values) {
        if (!std::isfinite(value))
            throw std::invalid_argument("Cylinder parameters must be finite numbers");
    }
    if (radius <= Precision::Confusion() || height <= Precision::Confusion())
        throw std::invalid_argument("Cylinder radius and height must be greater than tolerance");

    const gp_Vec axis(direction_x, direction_y, direction_z);
    if (axis.Magnitude() <= Precision::Confusion())
        throw std::invalid_argument("Cylinder axis direction must not be zero");

    // gp_Ax2 的原点是底面圆心，主方向是圆柱从底面指向顶面的轴向。
    const gp_Ax2 placement(
        gp_Pnt(center_x, center_y, center_z),
        gp_Dir(axis));
    BRepPrimAPI_MakeCylinder builder(placement, radius, height);
    builder.Build();
    if (!builder.IsDone())
        throw std::runtime_error("OpenCASCADE failed to create the cylinder");

    TopoDS_Shape shape = builder.Shape();
    if (shape.IsNull())
        throw std::runtime_error("OpenCASCADE returned an empty cylinder");
    if (shape.ShapeType() != TopAbs_SOLID)
        throw std::runtime_error("OpenCASCADE did not create a cylinder solid");
    if (!BRepCheck_Analyzer(shape).IsValid())
        throw std::runtime_error("The created cylinder is topologically invalid");
    return shape;
}

TopoDS_Shape GeometryBuilder::extrudeFace(
    const TopoDS_Face& face,
    double direction_x,
    double direction_y,
    double direction_z,
    double length)
{
    if (face.IsNull())
        throw std::invalid_argument("Extrude source face must not be null");

    const std::array<double, 4> values {
        direction_x, direction_y, direction_z, length
    };
    for (double value : values) {
        if (!std::isfinite(value))
            throw std::invalid_argument("Extrude parameters must be finite numbers");
    }
    if (length <= Precision::Confusion())
        throw std::invalid_argument("Extrude length must be greater than tolerance");

    const gp_Vec axis(direction_x, direction_y, direction_z);
    if (axis.Magnitude() <= Precision::Confusion())
        throw std::invalid_argument("Extrude direction must not be zero");

    gp_Vec extrusion { gp_Dir(axis) };
    extrusion.Multiply(length);

    // Copy=true：Solid 使用源面的副本作为底面，Component 中的独立源 Face 保持不变。
    BRepPrimAPI_MakePrism builder(face, extrusion, true, true);
    builder.Build();
    if (!builder.IsDone())
        throw std::runtime_error("OpenCASCADE failed to extrude the face");

    TopoDS_Shape shape = builder.Shape();
    if (shape.IsNull())
        throw std::runtime_error("OpenCASCADE returned an empty extrude result");
    if (!TopExp_Explorer(shape, TopAbs_SOLID).More())
        throw std::runtime_error("OpenCASCADE did not create an extruded solid");
    if (!BRepCheck_Analyzer(shape).IsValid())
        throw std::runtime_error("The extruded solid is topologically invalid");
    return shape;
}
