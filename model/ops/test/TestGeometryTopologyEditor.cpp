#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "GeometryTopologyEditor.h"

#include <BRepCheck_Analyzer.hxx>
#include <BRepAlgoAPI_Cut.hxx>
#include <BRep_Tool.hxx>
#include <BRep_Builder.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Pnt.hxx>
#include <GeomConvert.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Plane.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <Geom_Surface.hxx>
#include <gp_Ax3.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
/**
 * @brief 按模型层约束将输入形状包装为严格一层扁平的根 Compound。
 */
TopoDS_Shape makeGeometryRoot(TopoDS_Shape shape)
{
    GeometryData geometry;
    geometry.setRootShape(std::move(shape));
    return *geometry.rootShape;
}

int countSubshapes(const TopoDS_Shape& shape, TopAbs_ShapeEnum type)
{
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> shapes;
    TopExp::MapShapes(shape, type, shapes);
    return shapes.Extent();
}

/**
 * @brief 在形状中查找全部顶点都位于指定 Z 平面的 Face。
 */
std::vector<TopoDS_Face> findFacesOnZPlane(const TopoDS_Shape& shape, double z)
{
    constexpr double tolerance = 1.0e-7;
    std::vector<TopoDS_Face> faces;
    for (TopExp_Explorer face_exp(shape, TopAbs_FACE); face_exp.More(); face_exp.Next()) {
        const TopoDS_Face face = TopoDS::Face(face_exp.Current());
        bool on_plane = true;
        for (TopExp_Explorer vertex_exp(face, TopAbs_VERTEX); vertex_exp.More(); vertex_exp.Next()) {
            const gp_Pnt point = BRep_Tool::Pnt(TopoDS::Vertex(vertex_exp.Current()));
            if (std::abs(point.Z() - z) > tolerance) {
                on_plane = false;
                break;
            }
        }
        if (on_plane)
            faces.push_back(face);
    }
    return faces;
}

/**
 * @brief 在形状中查找全部顶点都位于指定 Z 平面的第一个 Face。
 */
TopoDS_Face findFaceOnZPlane(const TopoDS_Shape& shape, double z)
{
    const std::vector<TopoDS_Face> faces = findFacesOnZPlane(shape, z);
    return faces.empty() ? TopoDS_Face {} : faces.front();
}

/**
 * @brief 按坐标查找 Face 上已有的拓扑顶点，供分割边共享边界顶点。
 */
TopoDS_Vertex findFaceVertex(const TopoDS_Face& face, double x, double y, double z)
{
    constexpr double tolerance = 1.0e-7;
    const gp_Pnt expected(x, y, z);
    for (TopExp_Explorer vertex_exp(face, TopAbs_VERTEX); vertex_exp.More(); vertex_exp.Next()) {
        const TopoDS_Vertex vertex = TopoDS::Vertex(vertex_exp.Current());
        if (BRep_Tool::Pnt(vertex).Distance(expected) <= tolerance)
            return vertex;
    }
    return {};
}

/**
 * @brief 构造支撑曲面为 B-Spline 的面，用于验证同域判断不依赖曲面是否为解析曲面。
 *
 * GeomConvert 不接受无限曲面，因此先用矩形裁剪，再转换为 B-Spline。
 */
TopoDS_Face makeBsplineSupportFace(
    const occ::handle<Geom_Surface>& surface,
    double u_min,
    double u_max,
    double v_min,
    double v_max)
{
    const occ::handle<Geom_Surface> trimmed
        = new Geom_RectangularTrimmedSurface(surface, u_min, u_max, v_min, v_max);
    const occ::handle<Geom_BSplineSurface> bspline
        = GeomConvert::SurfaceToBSplineSurface(trimmed);
    return BRepBuilderAPI_MakeFace(bspline, 1.0e-7);
}

/**
 * @brief 构造指定轴向、半径与参数范围（u 为角度、v 为高度）的圆柱面。
 */
TopoDS_Face makeCylindricalFace(
    const gp_Ax3& axis,
    double radius,
    double u_min,
    double u_max,
    double v_min,
    double v_max)
{
    return BRepBuilderAPI_MakeFace(gp_Cylinder(axis, radius), u_min, u_max, v_min, v_max);
}

/**
 * @brief 把若干形状里的 Face 取出为自由面，包装成根 Compound。
 *
 * 把面取出为自由 Surface，供 Geometry Interference Check 单独验证面对求交。
 */
TopoDS_Shape makeFreeFacesRoot(std::initializer_list<const TopoDS_Shape*> shapes)
{
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    for (const TopoDS_Shape* shape : shapes) {
        for (TopExp_Explorer it(*shape, TopAbs_FACE); it.More(); it.Next())
            builder.Add(compound, it.Current());
    }
    return makeGeometryRoot(compound);
}

/**
 * @brief 把两个形状包装为根 Compound，供相交面诊断逐对比较。
 */
TopoDS_Shape makeShapePairRoot(const TopoDS_Shape& first, const TopoDS_Shape& second)
{
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, first);
    builder.Add(compound, second);
    return makeGeometryRoot(compound);
}

/**
 * @brief 把给定 Face 放进同一个诊断用 Solid，验证 Self Intersections 的实体范围。
 *
 * 该 Solid 不要求闭合，因为测试目标只是锁定 Face 到 Solid 的真实拓扑归属。
 */
TopoDS_Shape makeDiagnosticSolid(std::initializer_list<const TopoDS_Shape*> faces)
{
    BRep_Builder builder;
    TopoDS_Shell shell;
    builder.MakeShell(shell);
    for (const TopoDS_Shape* face : faces)
        builder.Add(shell, *face);
    TopoDS_Solid solid;
    builder.MakeSolid(solid);
    builder.Add(solid, shell);
    return makeGeometryRoot(solid);
}

/**
 * @brief 在任意支撑平面上构造矩形面，用于验证斜置面与远离原点的面判定。
 */
TopoDS_Face makePlanarRectangleFace(
    const gp_Pln& plane,
    double u_min,
    double u_max,
    double v_min,
    double v_max)
{
    return BRepBuilderAPI_MakeFace(plane, u_min, u_max, v_min, v_max);
}

/**
 * @brief 相交面诊断的选项：只保留重复面与相交面判定。
 */
GeometryTopologyDiagnosticOptions interferenceOptions()
{
    return GeometryTopologyDiagnosticOptions { false, false, false, true, false, true, false };
}
}

TEST_CASE("GeometryTopologyEditor splits a nested solid face with an on-face edge")
{
    const TopoDS_Shape box =
        GeometryBuilder::makeBox(0.0, 0.0, 0.0, 10.0, 20.0, 30.0);
    const TopoDS_Face bottom_face = findFaceOnZPlane(box, 0.0);
    REQUIRE_FALSE(bottom_face.IsNull());

    const TopoDS_Vertex first = findFaceVertex(bottom_face, 0.0, 0.0, 0.0);
    const TopoDS_Vertex second = findFaceVertex(bottom_face, 10.0, 20.0, 0.0);
    REQUIRE_FALSE(first.IsNull());
    REQUIRE_FALSE(second.IsNull());
    const TopoDS_Edge diagonal = TopoDS::Edge(GeometryBuilder::makeLine(first, second));

    const TopoDS_Shape root = makeGeometryRoot(box);
    const TopoDS_Shape result = GeometryTopologyEditor::splitFace(
        root, bottom_face, std::vector<TopoDS_Edge> { diagonal });

    REQUIRE_FALSE(result.IsNull());
    REQUIRE(countSubshapes(root, TopAbs_FACE) == 6);
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 7);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
}

TEST_CASE("GeometryTopologyEditor diagnoses boundary and isolated edges")
{
    const TopoDS_Shape face = GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 5.0, CoordinatePlane::XY);
    const TopoDS_Shape isolated_edge = GeometryBuilder::makeLine(
        20.0, 0.0, 0.0, 30.0, 0.0, 0.0);

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, face);
    builder.Add(compound, isolated_edge);

    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeGeometryRoot(compound), 1.0e-6, 1.0e-12);

    REQUIRE(result.boundary_edges.size() == 4);
    REQUIRE(result.isolated_edges.size() == 1);
    REQUIRE(result.non_manifold_edges.empty());
}

TEST_CASE("GeometryTopologyEditor groups geometrically duplicate faces")
{
    const TopoDS_Shape first = GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 5.0, CoordinatePlane::XY);
    const TopoDS_Shape second = GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 5.0, CoordinatePlane::XY);

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, first);
    builder.Add(compound, second);

    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeGeometryRoot(compound), 1.0e-6, 1.0e-12);

    REQUIRE(result.duplicate_face_groups.size() == 1);
    REQUIRE(result.duplicate_face_groups.front().faces.size() == 2);
    REQUIRE(result.self_intersecting_face_pairs.empty());
}

TEST_CASE("GeometryTopologyEditor diagnoses duplicate faces sharing boundary edges")
{
    const TopoDS_Face first = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 5.0, CoordinatePlane::XY));
    TopExp_Explorer wire(first, TopAbs_WIRE);
    REQUIRE(wire.More());
    const TopoDS_Face second = BRepBuilderAPI_MakeFace(TopoDS::Wire(wire.Current()));

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, first);
    builder.Add(compound, second);

    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeGeometryRoot(compound), 1.0e-6, 1.0e-12);

    REQUIRE(result.duplicate_face_groups.size() == 1);
    REQUIRE(result.duplicate_face_groups.front().faces.size() == 2);
    REQUIRE(result.interfering_face_pairs.empty());
}

TEST_CASE("GeometryTopologyEditor diagnoses crossing faces")
{
    const TopoDS_Shape first = GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY);
    const TopoDS_Shape second = GeometryBuilder::makeRectangleFace(
        0.0, 5.0, -5.0, 10.0, 10.0, CoordinatePlane::XZ);

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, first);
    builder.Add(compound, second);

    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeGeometryRoot(compound), 1.0e-6, 1.0e-12);

    REQUIRE(result.duplicate_face_groups.empty());
    REQUIRE(result.interfering_face_pairs.size() == 1);
}

TEST_CASE("GeometryTopologyEditor diagnoses crossing faces in one solid as self intersections")
{
    const TopoDS_Shape first = GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY);
    const TopoDS_Shape second = GeometryBuilder::makeRectangleFace(
        0.0, 5.0, -5.0, 10.0, 10.0, CoordinatePlane::XZ);

    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeDiagnosticSolid({ &first, &second }), 1.0e-6, 1.0e-12,
            GeometryTopologyDiagnosticOptions { false, false, false, true, true, true, false });

    REQUIRE(result.self_intersecting_face_pairs.size() == 1);
    REQUIRE(result.interfering_face_pairs.empty());
}

TEST_CASE("GeometryTopologyEditor diagnoses intersecting curved faces")
{
    const double full_angle = 2.0 * std::acos(-1.0);
    const TopoDS_Shape first = GeometryBuilder::makeCylinder(
        0.0, 0.0, -5.0, 2.0, 10.0, 0.0, 0.0, 1.0, full_angle);
    const TopoDS_Shape second = GeometryBuilder::makeCylinder(
        -5.0, 0.0, 0.0, 2.0, 10.0, 1.0, 0.0, 0.0, full_angle);

    // 两个圆柱的面取出为自由 Surface，验证 Geometry Interference Check 的曲面求交。
    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeFreeFacesRoot({ &first, &second }), 1.0e-6, 1.0e-12,
            GeometryTopologyDiagnosticOptions { false, false, false, false, false, true, false });

    REQUIRE_FALSE(result.interfering_face_pairs.empty());
}

TEST_CASE("GeometryTopologyEditor ignores independent faces sharing a geometric edge")
{
    const TopoDS_Shape first = GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY);
    const TopoDS_Shape second = GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XZ);

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, first);
    builder.Add(compound, second);

    GeometryTopologyDiagnosticOptions options {
        false, false, false, false, false, true, false
    };
    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeGeometryRoot(compound), 1.0e-6, 1.0e-12, options);

    REQUIRE(result.interfering_face_pairs.empty());
}

TEST_CASE("GeometryTopologyEditor excludes hidden duplicate faces from intersections")
{
    const TopoDS_Shape first = GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 5.0, CoordinatePlane::XY);
    const TopoDS_Shape second = GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 5.0, CoordinatePlane::XY);

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, first);
    builder.Add(compound, second);

    GeometryTopologyDiagnosticOptions options {
        false, false, false, false, false, true, false
    };
    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeGeometryRoot(compound), 1.0e-6, 1.0e-12, options);

    REQUIRE(result.duplicate_face_groups.empty());
    REQUIRE(result.interfering_face_pairs.empty());
}

TEST_CASE("GeometryTopologyEditor diagnoses partially overlapping coplanar faces")
{
    const TopoDS_Shape first = GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 5.0, CoordinatePlane::XY);
    const TopoDS_Shape second = GeometryBuilder::makeRectangleFace(
        5.0, 0.0, 0.0, 10.0, 5.0, CoordinatePlane::XY);

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, first);
    builder.Add(compound, second);

    GeometryTopologyDiagnosticOptions options {
        false, false, false, true, false, true, false
    };
    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeGeometryRoot(compound), 1.0e-6, 1.0e-12, options);

    REQUIRE(result.duplicate_face_groups.empty());
    REQUIRE(result.interfering_face_pairs.size() == 1);
}

TEST_CASE("GeometryTopologyEditor ignores coplanar faces touching at one edge")
{
    const TopoDS_Shape first = GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 5.0, CoordinatePlane::XY);
    const TopoDS_Shape second = GeometryBuilder::makeRectangleFace(
        10.0, 0.0, 0.0, 10.0, 5.0, CoordinatePlane::XY);

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, first);
    builder.Add(compound, second);

    GeometryTopologyDiagnosticOptions options {
        false, false, false, true, false, true, false
    };
    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeGeometryRoot(compound), 1.0e-6, 1.0e-12, options);

    REQUIRE(result.duplicate_face_groups.empty());
    REQUIRE(result.interfering_face_pairs.empty());
}

TEST_CASE("GeometryTopologyEditor diagnoses overlapping planar and B-spline support faces")
{
    // 两面共享 2D 区域时支撑曲面必然重合并由 Section 给出重合区域边界，
    // 该用例锁定这一前提：两张同域面必须被判为相交面。
    const TopoDS_Shape planar = GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 5.0, CoordinatePlane::XY);
    const TopoDS_Shape bspline = makeBsplineSupportFace(
        new Geom_Plane(gp_Pln(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0))),
        5.0, 15.0, 0.0, 5.0);

    GeometryTopologyDiagnosticOptions options {
        false, false, false, true, false, true, false
    };
    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeShapePairRoot(planar, bspline), 1.0e-6, 1.0e-12, options);

    REQUIRE(result.duplicate_face_groups.empty());
    REQUIRE(result.interfering_face_pairs.size() == 1);
}

TEST_CASE("GeometryTopologyEditor ignores disjoint coplanar B-spline support faces")
{
    const TopoDS_Shape planar = GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 5.0, CoordinatePlane::XY);
    const TopoDS_Shape bspline = makeBsplineSupportFace(
        new Geom_Plane(gp_Pln(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0))),
        20.0, 30.0, 0.0, 5.0);

    GeometryTopologyDiagnosticOptions options {
        false, false, false, true, false, true, false
    };
    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeShapePairRoot(planar, bspline), 1.0e-6, 1.0e-12, options);

    REQUIRE(result.duplicate_face_groups.empty());
    REQUIRE(result.interfering_face_pairs.empty());
}

TEST_CASE("GeometryTopologyEditor diagnoses overlapping coaxial cylindrical and B-spline faces")
{
    const gp_Ax3 axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
    const TopoDS_Shape cylinder = makeCylindricalFace(axis, 10.0, 0.0, 3.0, 0.0, 20.0);
    const TopoDS_Shape bspline = makeBsplineSupportFace(
        new Geom_CylindricalSurface(axis, 10.0), 0.5, 2.5, 5.0, 15.0);

    GeometryTopologyDiagnosticOptions options {
        false, false, false, true, false, true, false
    };
    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeShapePairRoot(cylinder, bspline), 1.0e-6, 1.0e-12, options);

    REQUIRE(result.duplicate_face_groups.empty());
    REQUIRE(result.interfering_face_pairs.size() == 1);
}

TEST_CASE("GeometryTopologyEditor ignores coaxial cylindrical faces with disjoint ranges")
{
    const gp_Ax3 axis(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0));
    const TopoDS_Shape cylinder = makeCylindricalFace(axis, 10.0, 0.0, 3.0, 0.0, 20.0);
    const TopoDS_Shape bspline = makeBsplineSupportFace(
        new Geom_CylindricalSurface(axis, 10.0), 4.0, 6.0, 40.0, 50.0);

    GeometryTopologyDiagnosticOptions options {
        false, false, false, true, false, true, false
    };
    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeShapePairRoot(cylinder, bspline), 1.0e-6, 1.0e-12, options);

    REQUIRE(result.duplicate_face_groups.empty());
    REQUIRE(result.interfering_face_pairs.empty());
}

TEST_CASE("GeometryTopologyEditor diagnoses crossing slanted faces far from the origin")
{
    // 分离判据必须在带地理坐标量级的模型上保持保守：斜置长条面与穿过它的
    // 小面真实相交时，不允许被提前判定为分离而漏检。
    constexpr double offset = 2.4e6;
    const gp_Pnt origin(offset, offset, offset);
    const TopoDS_Shape strip = makePlanarRectangleFace(
        gp_Pln(origin, gp_Dir(1.0, 0.0, 1.0)), -200.0, 200.0, -1.0, 1.0);
    const TopoDS_Shape crossing = makePlanarRectangleFace(
        gp_Pln(origin, gp_Dir(1.0, 0.0, -1.0)), -20.0, 20.0, -20.0, 20.0);

    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeShapePairRoot(strip, crossing), 1.0e-6, 1.0e-12, interferenceOptions());

    REQUIRE(result.duplicate_face_groups.empty());
    REQUIRE(result.interfering_face_pairs.size() == 1);
}

TEST_CASE("GeometryTopologyEditor diagnoses a plane crossing a B-spline patch far from the origin")
{
    // 自由曲面的保守边界按节点区间取控制点：多区间曲面在远离原点时仍必须
    // 完整覆盖曲面，不能因为控制点下标错位而漏掉真实相交。
    constexpr double offset = 2.4e6;
    const gp_Ax3 axis(gp_Pnt(offset, offset, offset), gp_Dir(0.0, 0.0, 1.0));
    const TopoDS_Shape patch = makeBsplineSupportFace(
        new Geom_CylindricalSurface(axis, 10.0), 0.0, 3.0, 0.0, 20.0);
    const TopoDS_Shape plane = makePlanarRectangleFace(
        gp_Pln(gp_Pnt(offset, offset, offset + 10.0), gp_Dir(0.0, 0.0, 1.0)), -40.0, 40.0, -40.0,
        40.0);

    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeShapePairRoot(patch, plane), 1.0e-6, 1.0e-12, interferenceOptions());

    REQUIRE(result.duplicate_face_groups.empty());
    REQUIRE(result.interfering_face_pairs.size() == 1);
}

TEST_CASE("GeometryTopologyEditor diagnoses intersecting cylinders far from the origin")
{
    constexpr double offset = 2.4e6;
    const double full_angle = 2.0 * std::acos(-1.0);
    const TopoDS_Shape first = GeometryBuilder::makeCylinder(
        offset, offset, offset - 5.0, 2.0, 10.0, 0.0, 0.0, 1.0, full_angle);
    const TopoDS_Shape second = GeometryBuilder::makeCylinder(
        offset - 5.0, offset, offset, 2.0, 10.0, 1.0, 0.0, 0.0, full_angle);

    // 把面取出为自由 Surface，在地理坐标量级下验证几何干涉判定不漏。
    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeFreeFacesRoot({ &first, &second }), 1.0e-6, 1.0e-12, interferenceOptions());

    REQUIRE_FALSE(result.interfering_face_pairs.empty());
}

TEST_CASE("GeometryTopologyEditor reports faces of interpenetrating solids")
{
    // 两个独立 Solid 互相咬穿时属于 Geometry Interference Check，不应并成一个零件。
    const double full_angle = 2.0 * std::acos(-1.0);
    const TopoDS_Shape first = GeometryBuilder::makeCylinder(
        0.0, 0.0, -5.0, 2.0, 10.0, 0.0, 0.0, 1.0, full_angle);
    const TopoDS_Shape second = GeometryBuilder::makeCylinder(
        -5.0, 0.0, 0.0, 2.0, 10.0, 1.0, 0.0, 0.0, full_angle);

    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeShapePairRoot(first, second), 1.0e-6, 1.0e-12, interferenceOptions());

    REQUIRE(result.self_intersecting_face_pairs.empty());
    REQUIRE_FALSE(result.interfering_face_pairs.empty());
}

TEST_CASE("GeometryTopologyEditor ignores faces of nested solids")
{
    // 一个 Solid 完整嵌在另一个内部时两者表面没有相交，不属于 Self Intersections，
    // Geometry Interference Check 也不报告。
    const double full_angle = 2.0 * std::acos(-1.0);
    const TopoDS_Shape outer = GeometryBuilder::makeCylinder(
        0.0, 0.0, -20.0, 20.0, 40.0, 0.0, 0.0, 1.0, full_angle);
    const TopoDS_Shape inner = GeometryBuilder::makeCylinder(
        0.0, 0.0, -5.0, 2.0, 10.0, 0.0, 0.0, 1.0, full_angle);

    GeometryTopologyDiagnosticOptions options = interferenceOptions();
    options.interfering_faces = true;
    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeShapePairRoot(outer, inner), 1.0e-6, 1.0e-12, options);

    REQUIRE(result.self_intersecting_face_pairs.empty());
    REQUIRE(result.interfering_face_pairs.empty());
}

TEST_CASE("GeometryTopologyEditor reports tangent nested solids as interference")
{
    // 内柱嵌在圆管空腔内、侧面与管内壁的间距远小于 Confusion（airplane 的内外两层
    // 蒙皮即此情形）。前处理器的 Geometry Interference Check 把"重叠面"也计入
    // intersection（官方对 overlapping surfaces 的判据是两面法向夹角 < 10 度或 > 170 度），
    // 因此容差内贴合要报干涉。注意这与「一个实体完整包住另一个、且面互不相交」不同，
    // 那类仍然不报（见 ignores faces of nested solids）。
    // 内柱比管短，保证贴合面因面积悬殊不会被当成重复面而提前分流。
    const double full_angle = 2.0 * std::acos(-1.0);
    const TopoDS_Shape outer_cylinder = GeometryBuilder::makeCylinder(
        0.0, 0.0, -10.0, 5.0, 20.0, 0.0, 0.0, 1.0, full_angle);
    const TopoDS_Shape cavity = GeometryBuilder::makeCylinder(
        0.0, 0.0, -11.0, 4.0, 22.0, 0.0, 0.0, 1.0, full_angle);
    BRepAlgoAPI_Cut cutter(outer_cylinder, cavity);
    cutter.Build();
    REQUIRE(cutter.IsDone());
    const TopoDS_Shape outer = cutter.Shape();
    // 同时覆盖容差内微小间隙和完全贴合两种情形，两者都应被判为重叠。
    for (double radius : { 4.0 - 1.0e-9, 4.0 }) {
        const TopoDS_Shape inner = GeometryBuilder::makeCylinder(
            0.0, 0.0, -2.5, radius, 5.0, 0.0, 0.0, 1.0, full_angle);

        GeometryTopologyDiagnosticOptions options = interferenceOptions();
        options.interfering_faces = true;
        const GeometryTopologyDiagnosticResult result
            = GeometryTopologyEditor::diagnoseTopology(
                makeShapePairRoot(outer, inner), 1.0e-6, 1.0e-12, options);

        REQUIRE(result.self_intersecting_face_pairs.empty());
        REQUIRE_FALSE(result.interfering_face_pairs.empty());
    }
}

TEST_CASE("GeometryTopologyEditor reports interference when a nested solid pierces the wall")
{
    // 「回」字形方管的空腔贯穿厚度方向；长条从空腔伸进壁材料但整体仍在管的包围盒
    // 内。它与管内壁的交线两侧一面进入对方材料，
    // 是真实穿透，必须报干涉；贴着对方边界滑过的接触面（如条顶面与管顶面共面）不报。
    const TopoDS_Shape outer_box = GeometryBuilder::makeBox(0.0, 0.0, 0.0, 20.0, 20.0, 10.0);
    const TopoDS_Shape cavity = GeometryBuilder::makeBox(8.0, 8.0, 0.0, 4.0, 4.0, 10.0);
    BRepAlgoAPI_Cut cutter(outer_box, cavity);
    cutter.Build();
    REQUIRE(cutter.IsDone());
    const TopoDS_Shape outer = cutter.Shape();
    const TopoDS_Shape inner = GeometryBuilder::makeBox(9.0, 9.0, 0.0, 6.0, 2.0, 10.0);

    GeometryTopologyDiagnosticOptions options = interferenceOptions();
    options.interfering_faces = true;
    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeShapePairRoot(outer, inner), 1.0e-6, 1.0e-12, options);

    REQUIRE(result.self_intersecting_face_pairs.empty());
    REQUIRE_FALSE(result.interfering_face_pairs.empty());
}

TEST_CASE("GeometryTopologyEditor ignores regular adjacent box faces")
{
    GeometryTopologyDiagnosticOptions options {
        false, false, false, true, true, false, false
    };
    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeGeometryRoot(GeometryBuilder::makeBox(
                0.0, 0.0, 0.0, 10.0, 20.0, 30.0)),
            1.0e-6, 1.0e-12, options);

    REQUIRE(result.duplicate_face_groups.empty());
    REQUIRE(result.self_intersecting_face_pairs.empty());
}

TEST_CASE("GeometryTopologyEditor diagnoses small edges and small faces independently")
{
    const TopoDS_Shape small_edge = GeometryBuilder::makeLine(
        0.0, 0.0, 0.0, 0.1, 0.0, 0.0);
    const TopoDS_Shape small_face = GeometryBuilder::makeRectangleFace(
        2.0, 0.0, 0.0, 1.0, 0.1, CoordinatePlane::XY);

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, small_edge);
    builder.Add(compound, small_face);

    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeGeometryRoot(compound), 0.2, 0.2);

    REQUIRE(result.small_edges.size() == 3);
    REQUIRE(result.small_faces.size() == 1);
}

TEST_CASE("GeometryTopologyEditor does not treat nearby parallel faces as duplicates")
{
    const TopoDS_Shape first = GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 5.0, CoordinatePlane::XY);
    const TopoDS_Shape second = GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.2, 10.0, 5.0, CoordinatePlane::XY);

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, first);
    builder.Add(compound, second);

    const GeometryTopologyDiagnosticResult result
        = GeometryTopologyEditor::diagnoseTopology(
            makeGeometryRoot(compound), 0.4, 0.16);

    REQUIRE(result.duplicate_face_groups.empty());
}

TEST_CASE("GeometryTopologyEditor rejects invalid face split inputs")
{
    const TopoDS_Face target_face = TopoDS::Face(
        GeometryBuilder::makeRectangleFace(
            0.0, 0.0, 0.0, 10.0, 20.0, CoordinatePlane::XY));
    const TopoDS_Shape root = makeGeometryRoot(target_face);

    REQUIRE_THROWS_AS(
        GeometryTopologyEditor::splitFace(root, target_face, {}),
        std::invalid_argument);

    const TopoDS_Face unrelated_face = TopoDS::Face(
        GeometryBuilder::makeRectangleFace(
            30.0, 0.0, 0.0, 10.0, 20.0, CoordinatePlane::XY));
    const TopoDS_Edge edge = TopoDS::Edge(
        GeometryBuilder::makeLine(30.0, 0.0, 0.0, 40.0, 20.0, 0.0));
    REQUIRE_THROWS_AS(
        GeometryTopologyEditor::splitFace(
            root, unrelated_face, std::vector<TopoDS_Edge> { edge }),
        std::invalid_argument);

    const TopoDS_Edge off_face_edge = TopoDS::Edge(
        GeometryBuilder::makeLine(0.0, 0.0, 1.0, 10.0, 20.0, 1.0));
    REQUIRE_THROWS_AS(
        GeometryTopologyEditor::splitFace(
            root, target_face, std::vector<TopoDS_Edge> { off_face_edge }),
        std::invalid_argument);
}

TEST_CASE("GeometryTopologyEditor splits an edge by ratio")
{
    const TopoDS_Edge edge = TopoDS::Edge(
        GeometryBuilder::makeLine(0.0, 0.0, 0.0, 10.0, 0.0, 0.0));
    const TopoDS_Shape root = makeGeometryRoot(edge);
    const TopoDS_Shape result = GeometryTopologyEditor::splitEdge(root, edge, 0.25);

    REQUIRE(countSubshapes(result, TopAbs_EDGE) == 2);
    REQUIRE(countSubshapes(result, TopAbs_VERTEX) == 3);
    bool found_split_point = false;
    for (TopExp_Explorer it(result, TopAbs_VERTEX); it.More(); it.Next()) {
        if (BRep_Tool::Pnt(TopoDS::Vertex(it.Current())).Distance(gp_Pnt(2.5, 0.0, 0.0)) < 1.0e-7)
            found_split_point = true;
    }
    REQUIRE(found_split_point);
}

TEST_CASE("GeometryTopologyEditor collapses an isolated edge to its midpoint")
{
    const TopoDS_Edge edge = TopoDS::Edge(
        GeometryBuilder::makeLine(0.0, 0.0, 0.0, 10.0, 0.0, 0.0));
    const TopoDS_Shape root = makeGeometryRoot(edge);
    const TopoDS_Shape result = GeometryTopologyEditor::collapseEdge(
        root, edge, gp_Pnt(5.0, 0.0, 0.0));

    REQUIRE(countSubshapes(result, TopAbs_EDGE) == 0);
    REQUIRE(countSubshapes(result, TopAbs_VERTEX) == 1);
    TopExp_Explorer vertex_exp(result, TopAbs_VERTEX);
    REQUIRE(vertex_exp.More());
    REQUIRE(BRep_Tool::Pnt(TopoDS::Vertex(vertex_exp.Current()))
                .Distance(gp_Pnt(5.0, 0.0, 0.0))
        < 1.0e-7);
}

TEST_CASE("GeometryTopologyEditor collapses parallel edges sharing both endpoints")
{
    const TopoDS_Vertex first = TopoDS::Vertex(GeometryBuilder::makePoint(0.0, 0.0, 0.0));
    const TopoDS_Vertex second = TopoDS::Vertex(GeometryBuilder::makePoint(10.0, 0.0, 0.0));
    const TopoDS_Edge selected_edge = TopoDS::Edge(GeometryBuilder::makeLine(first, second));
    const TopoDS_Edge parallel_edge = TopoDS::Edge(GeometryBuilder::makeLine(first, second));

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, selected_edge);
    builder.Add(compound, parallel_edge);
    const TopoDS_Shape root = makeGeometryRoot(compound);

    const TopoDS_Shape result = GeometryTopologyEditor::collapseEdge(
        root, selected_edge, gp_Pnt(5.0, 0.0, 0.0));

    REQUIRE(countSubshapes(result, TopAbs_EDGE) == 0);
    REQUIRE(countSubshapes(result, TopAbs_VERTEX) == 1);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
}

TEST_CASE("GeometryTopologyEditor collapses an edge between two junctions")
{
    const TopoDS_Vertex first = TopoDS::Vertex(GeometryBuilder::makePoint(0.0, 0.0, 0.0));
    const TopoDS_Vertex second = TopoDS::Vertex(GeometryBuilder::makePoint(1.0, 0.0, 0.0));
    const TopoDS_Edge short_edge = TopoDS::Edge(GeometryBuilder::makeLine(first, second));

    BRep_Builder builder;
    TopoDS_Compound shapes;
    builder.MakeCompound(shapes);
    builder.Add(shapes, short_edge);
    builder.Add(shapes, GeometryBuilder::makeLine(first,
                            TopoDS::Vertex(GeometryBuilder::makePoint(-10.0, 0.0, 0.0))));
    builder.Add(shapes, GeometryBuilder::makeLine(first,
                            TopoDS::Vertex(GeometryBuilder::makePoint(0.0, 10.0, 0.0))));
    builder.Add(shapes, GeometryBuilder::makeLine(second,
                            TopoDS::Vertex(GeometryBuilder::makePoint(10.0, 0.0, 0.0))));
    builder.Add(shapes, GeometryBuilder::makeLine(second,
                            TopoDS::Vertex(GeometryBuilder::makePoint(1.0, -10.0, 0.0))));
    const TopoDS_Shape root = makeGeometryRoot(shapes);

    for (const gp_Pnt& target : { gp_Pnt(0.0, 0.0, 0.0),
             gp_Pnt(1.0, 0.0, 0.0), gp_Pnt(0.5, 0.0, 0.0) }) {
        const TopoDS_Shape result = GeometryTopologyEditor::collapseEdge(root, short_edge, target);
        REQUIRE(countSubshapes(result, TopAbs_EDGE) == 4);
        REQUIRE(countSubshapes(result, TopAbs_VERTEX) == 5);
        REQUIRE(BRepCheck_Analyzer(result).IsValid());
    }
}

TEST_CASE("GeometryTopologyEditor keeps rectangle boundary when deleting a face")
{
    const TopoDS_Face face = TopoDS::Face(
        GeometryBuilder::makeRectangleFace(
            0.0, 0.0, 0.0, 10.0, 20.0, CoordinatePlane::XY));
    const TopoDS_Shape root = makeGeometryRoot(face);

    const TopoDS_Shape result =
        GeometryTopologyEditor::removeTopLevelShape(root, face, false);

    REQUIRE_FALSE(result.IsNull());
    REQUIRE(result.ShapeType() == TopAbs_COMPOUND);
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 0);
    REQUIRE(countSubshapes(result, TopAbs_EDGE) == 4);
    REQUIRE(countSubshapes(result, TopAbs_VERTEX) == 4);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
}

TEST_CASE("GeometryTopologyEditor cascades deletion of an isolated face")
{
    const TopoDS_Face face = TopoDS::Face(
        GeometryBuilder::makeRectangleFace(
            0.0, 0.0, 0.0, 10.0, 20.0, CoordinatePlane::XY));
    const TopoDS_Shape root = makeGeometryRoot(face);

    const TopoDS_Shape result =
        GeometryTopologyEditor::removeTopLevelShape(root, face, true);

    REQUIRE(result.IsNull());
}

TEST_CASE("GeometryTopologyEditor preserves other root compound children")
{
    const TopoDS_Face face = TopoDS::Face(
        GeometryBuilder::makeRectangleFace(
            0.0, 0.0, 0.0, 10.0, 20.0, CoordinatePlane::XY));
    const TopoDS_Shape point = GeometryBuilder::makePoint(30.0, 0.0, 0.0);

    BRep_Builder builder;
    TopoDS_Compound root;
    builder.MakeCompound(root);
    builder.Add(root, face);
    builder.Add(root, point);
    const TopoDS_Shape geometry_root = makeGeometryRoot(root);

    const TopoDS_Shape result =
        GeometryTopologyEditor::removeTopLevelShape(geometry_root, face, true);

    REQUIRE_FALSE(result.IsNull());
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 0);
    REQUIRE(countSubshapes(result, TopAbs_VERTEX) == 1);
}

TEST_CASE("GeometryTopologyEditor rejects a face nested in a solid")
{
    const TopoDS_Shape box =
        GeometryBuilder::makeBox(0.0, 0.0, 0.0, 10.0, 20.0, 30.0);
    TopExp_Explorer face_exp(box, TopAbs_FACE);
    REQUIRE(face_exp.More());
    const TopoDS_Face nested_face = TopoDS::Face(face_exp.Current());
    const TopoDS_Shape root = makeGeometryRoot(box);

    REQUIRE_THROWS_AS(
        GeometryTopologyEditor::removeTopLevelShape(root, nested_face, false),
        std::invalid_argument);
}

TEST_CASE("GeometryTopologyEditor keeps edge vertices")
{
    const TopoDS_Shape edge =
        GeometryBuilder::makeLine(0.0, 0.0, 0.0, 10.0, 0.0, 0.0);
    const TopoDS_Shape root = makeGeometryRoot(edge);

    const TopoDS_Shape result =
        GeometryTopologyEditor::removeTopLevelShape(root, edge, false);

    REQUIRE_FALSE(result.IsNull());
    REQUIRE(countSubshapes(result, TopAbs_EDGE) == 0);
    REQUIRE(countSubshapes(result, TopAbs_VERTEX) == 2);
}

TEST_CASE("GeometryTopologyEditor deletes one top-level vertex")
{
    const TopoDS_Shape first = GeometryBuilder::makePoint(0.0, 0.0, 0.0);
    const TopoDS_Shape second = GeometryBuilder::makePoint(10.0, 0.0, 0.0);

    BRep_Builder builder;
    TopoDS_Compound root;
    builder.MakeCompound(root);
    builder.Add(root, first);
    builder.Add(root, second);
    const TopoDS_Shape geometry_root = makeGeometryRoot(root);

    const TopoDS_Shape result =
        GeometryTopologyEditor::removeTopLevelShape(
            geometry_root, first, false);

    REQUIRE_FALSE(result.IsNull());
    REQUIRE(countSubshapes(result, TopAbs_VERTEX) == 1);
}

TEST_CASE("GeometryTopologyEditor keeps solid boundary faces")
{
    const TopoDS_Shape solid =
        GeometryBuilder::makeBox(0.0, 0.0, 0.0, 10.0, 20.0, 30.0);
    const TopoDS_Shape root = makeGeometryRoot(solid);

    const TopoDS_Shape result =
        GeometryTopologyEditor::removeTopLevelShape(root, solid, false);

    REQUIRE_FALSE(result.IsNull());
    REQUIRE(countSubshapes(result, TopAbs_SOLID) == 0);
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 6);
    REQUIRE(countSubshapes(result, TopAbs_EDGE) == 12);
    REQUIRE(countSubshapes(result, TopAbs_VERTEX) == 8);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
}
