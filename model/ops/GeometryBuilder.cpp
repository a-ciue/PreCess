#include "GeometryBuilder.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_WireError.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepFill_Filling.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakePrism.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
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

// 统一检查完整圆柱与部分圆柱的构造结果，避免两个重载分支重复错误处理。
TopoDS_Shape checkedCylinder(BRepPrimAPI_MakeCylinder& builder)
{
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

// 统一检查完整圆锥和部分圆锥的构造结果，避免两个重载分支重复错误处理。
TopoDS_Shape checkedCone(BRepPrimAPI_MakeCone& builder)
{
    builder.Build();
    if (!builder.IsDone())
        throw std::runtime_error("OpenCASCADE failed to create the cone");

    TopoDS_Shape shape = builder.Shape();
    if (shape.IsNull())
        throw std::runtime_error("OpenCASCADE returned an empty cone");
    if (shape.ShapeType() != TopAbs_SOLID)
        throw std::runtime_error("OpenCASCADE did not create a cone solid");
    if (!BRepCheck_Analyzer(shape).IsValid())
        throw std::runtime_error("The created cone is topologically invalid");
    return shape;
}

// 统一检查完整球体和各类部分球体的构造结果，避免不同重载重复错误处理。
TopoDS_Shape checkedSphere(BRepPrimAPI_MakeSphere& builder)
{
    builder.Build();
    if (!builder.IsDone())
        throw std::runtime_error("OpenCASCADE failed to create the sphere");

    TopoDS_Shape shape = builder.Shape();
    if (shape.IsNull())
        throw std::runtime_error("OpenCASCADE returned an empty sphere");
    if (shape.ShapeType() != TopAbs_SOLID)
        throw std::runtime_error("OpenCASCADE did not create a sphere solid");
    if (!BRepCheck_Analyzer(shape).IsValid())
        throw std::runtime_error("The created sphere is topologically invalid");
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
    CoordinatePlane plane,
    double start_angle,
    double sweep_angle)
{
    const std::array<double, 6> values {
        center_x, center_y, center_z, radius, start_angle, sweep_angle
    };
    for (double value : values) {
        if (!std::isfinite(value))
            throw std::invalid_argument("Disk face parameters must be finite numbers");
    }
    if (radius <= Precision::Confusion())
        throw std::invalid_argument("Disk face radius must be greater than tolerance");

    const double full_angle = 2.0 * std::acos(-1.0);
    if (sweep_angle <= Precision::Angular()
        || sweep_angle > full_angle + Precision::Angular())
        throw std::invalid_argument("Disk face sweep angle must be in (0, 2*PI]");

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

    BRepBuilderAPI_MakeWire wire_builder;
    if (std::abs(sweep_angle - full_angle) <= Precision::Angular()) {
        // 完整圆盘使用整圆 Edge，避免在周期接缝处人为拆边。
        BRepBuilderAPI_MakeEdge circle_builder(gp_Circ(placement, radius));
        if (!circle_builder.IsDone())
            throw std::runtime_error("OpenCASCADE failed to create the disk edge");
        wire_builder.Add(circle_builder.Edge());
    } else {
        // 旋转局部 X 方向表达起始角，再以 [0, sweep] 创建圆弧，避免跨越 2*PI 参数边界。
        double normalized_start = std::fmod(start_angle, full_angle);
        if (normalized_start < 0.0)
            normalized_start += full_angle;
        placement.Rotate(placement.Axis(), normalized_start);

        BRepBuilderAPI_MakeEdge arc_builder(
            gp_Circ(placement, radius), 0.0, sweep_angle);
        if (!arc_builder.IsDone())
            throw std::runtime_error("OpenCASCADE failed to create the sector arc");

        BRepBuilderAPI_MakeVertex center_builder(center);
        if (!center_builder.IsDone())
            throw std::runtime_error("OpenCASCADE failed to create the sector center");

        // 复用圆弧端点构造两条半径边，保证三条 Edge 形成共享顶点的闭合 Wire。
        BRepBuilderAPI_MakeEdge end_radius_builder(
            arc_builder.Vertex2(), center_builder.Vertex());
        BRepBuilderAPI_MakeEdge start_radius_builder(
            center_builder.Vertex(), arc_builder.Vertex1());
        if (!end_radius_builder.IsDone() || !start_radius_builder.IsDone())
            throw std::runtime_error("OpenCASCADE failed to create the sector radius edges");

        wire_builder.Add(arc_builder.Edge());
        wire_builder.Add(end_radius_builder.Edge());
        wire_builder.Add(start_radius_builder.Edge());
    }
    if (!wire_builder.IsDone())
        throw std::runtime_error("OpenCASCADE failed to create the circular face wire");

    BRepBuilderAPI_MakeFace face_builder(wire_builder.Wire(), true);
    if (!face_builder.IsDone())
        throw std::runtime_error("OpenCASCADE failed to create the circular face");

    TopoDS_Shape shape = face_builder.Shape();
    if (shape.IsNull())
        throw std::runtime_error("OpenCASCADE returned an empty circular face");
    if (!BRepCheck_Analyzer(shape).IsValid())
        throw std::runtime_error("The created circular face is topologically invalid");
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
    double direction_z,
    double sweep_angle)
{
    const std::array<double, 9> values {
        center_x,
        center_y,
        center_z,
        radius,
        height,
        direction_x,
        direction_y,
        direction_z,
        sweep_angle
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

    const double full_angle = 2.0 * std::acos(-1.0);
    if (sweep_angle <= Precision::Angular()
        || sweep_angle > full_angle + Precision::Angular())
        throw std::invalid_argument("Cylinder sweep angle must be in (0, 2*PI]");

    // gp_Ax2 的原点是底面圆心，主方向是圆柱从底面指向顶面的轴向。
    const gp_Ax2 placement(
        gp_Pnt(center_x, center_y, center_z),
        gp_Dir(axis));
    // 完整圆柱调用无角度重载；部分圆柱由 OCC 自动补齐两个径向封闭面。
    if (std::abs(sweep_angle - full_angle) <= Precision::Angular()) {
        BRepPrimAPI_MakeCylinder builder(placement, radius, height);
        return checkedCylinder(builder);
    }

    BRepPrimAPI_MakeCylinder builder(placement, radius, height, sweep_angle);
    return checkedCylinder(builder);
}

TopoDS_Shape GeometryBuilder::makeCone(
    double center_x,
    double center_y,
    double center_z,
    double bottom_radius,
    double top_radius,
    double height,
    double direction_x,
    double direction_y,
    double direction_z,
    double sweep_angle)
{
    const std::array<double, 10> values {
        center_x,
        center_y,
        center_z,
        bottom_radius,
        top_radius,
        height,
        direction_x,
        direction_y,
        direction_z,
        sweep_angle
    };
    for (double value : values) {
        if (!std::isfinite(value))
            throw std::invalid_argument("Cone parameters must be finite numbers");
    }

    if (bottom_radius < 0.0 || top_radius < 0.0)
        throw std::invalid_argument("Cone radii must not be negative");
    if (bottom_radius <= Precision::Confusion()
        && top_radius <= Precision::Confusion())
        throw std::invalid_argument("At least one cone radius must be greater than tolerance");
    if (std::abs(bottom_radius - top_radius) <= Precision::Confusion())
        throw std::invalid_argument("Cone radii must be different; use a cylinder instead");
    if (height <= Precision::Confusion())
        throw std::invalid_argument("Cone height must be greater than tolerance");

    const gp_Vec axis(direction_x, direction_y, direction_z);
    if (axis.Magnitude() <= Precision::Confusion())
        throw std::invalid_argument("Cone axis direction must not be zero");

    const double full_angle = 2.0 * std::acos(-1.0);
    if (sweep_angle <= Precision::Angular()
        || sweep_angle > full_angle + Precision::Angular())
        throw std::invalid_argument("Cone sweep angle must be in (0, 2*PI]");

    // 底面圆心和轴向共同定义局部坐标系，两个半径分别位于 z=0 和 z=height。
    const gp_Ax2 placement(
        gp_Pnt(center_x, center_y, center_z),
        gp_Dir(axis));
    // 完整体使用无角度重载；部分体由 OCC 自动增加两个径向封闭面。
    if (std::abs(sweep_angle - full_angle) <= Precision::Angular()) {
        BRepPrimAPI_MakeCone builder(
            placement, bottom_radius, top_radius, height);
        return checkedCone(builder);
    }

    BRepPrimAPI_MakeCone builder(
        placement, bottom_radius, top_radius, height, sweep_angle);
    return checkedCone(builder);
}

TopoDS_Shape GeometryBuilder::makeSphere(
    double center_x,
    double center_y,
    double center_z,
    double radius,
    double direction_x,
    double direction_y,
    double direction_z,
    double minimum_latitude,
    double maximum_latitude,
    double longitude_sweep)
{
    const std::array<double, 10> values {
        center_x,
        center_y,
        center_z,
        radius,
        direction_x,
        direction_y,
        direction_z,
        minimum_latitude,
        maximum_latitude,
        longitude_sweep
    };
    for (double value : values) {
        if (!std::isfinite(value))
            throw std::invalid_argument("Sphere parameters must be finite numbers");
    }

    if (radius <= Precision::Confusion())
        throw std::invalid_argument("Sphere radius must be greater than tolerance");

    const gp_Vec axis(direction_x, direction_y, direction_z);
    if (axis.Magnitude() <= Precision::Confusion())
        throw std::invalid_argument("Sphere axis direction must not be zero");

    const double pi = std::acos(-1.0);
    const double half_pi = pi / 2.0;
    const double full_angle = 2.0 * pi;
    if (minimum_latitude < -half_pi - Precision::Angular()
        || maximum_latitude > half_pi + Precision::Angular())
        throw std::invalid_argument("Sphere latitude angles must be in [-PI/2, PI/2]");
    if (maximum_latitude - minimum_latitude <= Precision::Angular())
        throw std::invalid_argument("Sphere maximum latitude must be greater than minimum latitude");
    if (longitude_sweep <= Precision::Angular()
        || longitude_sweep > full_angle + Precision::Angular())
        throw std::invalid_argument("Sphere longitude sweep must be in (0, 2*PI]");

    // 将容差范围内的极点角度归一到精确值，避免 OCC 因浮点换算越过纬度边界。
    const double normalized_minimum =
        std::abs(minimum_latitude + half_pi) <= Precision::Angular()
        ? -half_pi
        : minimum_latitude;
    const double normalized_maximum =
        std::abs(maximum_latitude - half_pi) <= Precision::Angular()
        ? half_pi
        : maximum_latitude;
    const bool uses_full_latitude =
        normalized_minimum == -half_pi && normalized_maximum == half_pi;
    const bool uses_full_longitude =
        std::abs(longitude_sweep - full_angle) <= Precision::Angular();

    // 主方向定义南北极轴；部分球体的经度从 OCC 局部 X 参考方向开始扫掠。
    const gp_Ax2 placement(
        gp_Pnt(center_x, center_y, center_z),
        gp_Dir(axis));
    if (uses_full_latitude && uses_full_longitude) {
        BRepPrimAPI_MakeSphere builder(placement, radius);
        return checkedSphere(builder);
    }
    if (uses_full_latitude) {
        BRepPrimAPI_MakeSphere builder(placement, radius, longitude_sweep);
        return checkedSphere(builder);
    }
    if (uses_full_longitude) {
        BRepPrimAPI_MakeSphere builder(
            placement, radius, normalized_minimum, normalized_maximum);
        return checkedSphere(builder);
    }

    BRepPrimAPI_MakeSphere builder(
        placement,
        radius,
        normalized_minimum,
        normalized_maximum,
        longitude_sweep);
    return checkedSphere(builder);
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
