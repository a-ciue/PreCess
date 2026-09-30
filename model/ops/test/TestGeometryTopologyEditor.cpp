#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "GeometryTopologyEditor.h"

#include <BRepCheck_Analyzer.hxx>
#include <BRepTools.hxx>
#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepGProp.hxx>
#include <GProp_GProps.hxx>
#include <BRepAlgoAPI_Cut.hxx>
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
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Pnt.hxx>
#include <GeomConvert.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom_BezierCurve.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <Geom_CylindricalSurface.hxx>
#include <Geom_Plane.hxx>
#include <Geom_RectangularTrimmedSurface.hxx>
#include <Geom_Surface.hxx>
#include <gp_Ax3.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Dir.hxx>
#include <gp_Pln.hxx>

#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRep_Tool.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopoDS_Wire.hxx>
#include <gp_Ax2.hxx>
#include <gp_Circ.hxx>
#include <catch2/catch_approx.hpp>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_List.hxx>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <array>
#include <sstream>
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
 * @brief 查找两个端点都位于指定 X 坐标的边。
 */
TopoDS_Edge findEdgeOnX(const TopoDS_Face& face, double x)
{
    constexpr double tolerance = 1.0e-7;
    for (TopExp_Explorer edge_exp(face, TopAbs_EDGE); edge_exp.More(); edge_exp.Next()) {
        const TopoDS_Edge edge = TopoDS::Edge(edge_exp.Current());
        TopoDS_Vertex start;
        TopoDS_Vertex end;
        TopExp::Vertices(edge, start, end, true);
        if (!start.IsNull() && !end.IsNull()
            && std::abs(BRep_Tool::Pnt(start).X() - x) <= tolerance
            && std::abs(BRep_Tool::Pnt(end).X() - x) <= tolerance)
            return edge;
    }
    return {};
}

/**
 * @brief 创建左边界由上下两条边组成的矩形面。
 */
std::pair<TopoDS_Face, std::vector<TopoDS_Edge>> makeSplitLeftRectangleFace(
    double x,
    double y,
    double width,
    double height)
{
    const TopoDS_Vertex bottom = TopoDS::Vertex(GeometryBuilder::makePoint(x, y, 0.0));
    const TopoDS_Vertex middle = TopoDS::Vertex(
        GeometryBuilder::makePoint(x, y + height * 0.5, 0.0));
    const TopoDS_Vertex top = TopoDS::Vertex(GeometryBuilder::makePoint(x, y + height, 0.0));
    const TopoDS_Vertex top_right = TopoDS::Vertex(
        GeometryBuilder::makePoint(x + width, y + height, 0.0));
    const TopoDS_Vertex bottom_right = TopoDS::Vertex(
        GeometryBuilder::makePoint(x + width, y, 0.0));

    const TopoDS_Edge lower = TopoDS::Edge(GeometryBuilder::makeLine(bottom, middle));
    const TopoDS_Edge upper = TopoDS::Edge(GeometryBuilder::makeLine(middle, top));
    const TopoDS_Edge top_edge = TopoDS::Edge(GeometryBuilder::makeLine(top, top_right));
    const TopoDS_Edge right_edge = TopoDS::Edge(
        GeometryBuilder::makeLine(top_right, bottom_right));
    const TopoDS_Edge bottom_edge = TopoDS::Edge(
        GeometryBuilder::makeLine(bottom_right, bottom));

    BRepBuilderAPI_MakeWire wire_builder;
    for (const TopoDS_Edge& edge : { lower, upper, top_edge, right_edge, bottom_edge })
        wire_builder.Add(edge);
    if (!wire_builder.IsDone())
        throw std::runtime_error("Failed to build split-boundary test wire");
    BRepBuilderAPI_MakeFace face_builder(wire_builder.Wire());
    if (!face_builder.IsDone())
        throw std::runtime_error("Failed to build split-boundary test face");
    return { face_builder.Face(), { lower, upper } };
}

/**
 * @brief 统计同时邻接两个 Face 的共享边数量。
 */
int countSharedEdges(const TopoDS_Shape& shape)
{
    NCollection_IndexedDataMap<TopoDS_Shape,
        NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher>
        edge_faces;
    TopExp::MapShapesAndUniqueAncestors(shape, TopAbs_EDGE, TopAbs_FACE, edge_faces);
    int shared_count = 0;
    for (int index = 1; index <= edge_faces.Extent(); ++index) {
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faces;
        for (const TopoDS_Shape& face : edge_faces.FindFromIndex(index))
            faces.Add(face);
        if (faces.Extent() == 2)
            ++shared_count;
    }
    return shared_count;
}


}


TEST_CASE("GeometryTopologyEditor splits a face with an intersecting face")
{
    const TopoDS_Face target = TopoDS::Face(
        GeometryBuilder::makeRectangleFace(
            0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const TopoDS_Face tool = TopoDS::Face(
        GeometryBuilder::makeRectangleFace(
            0.0, 5.0, -5.0, 10.0, 10.0, CoordinatePlane::XZ));

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, target);
    builder.Add(compound, tool);
    const TopoDS_Shape root = makeGeometryRoot(compound);

    const TopoDS_Shape result = GeometryTopologyEditor::splitFaceByFaces(
        root, target, std::vector<TopoDS_Face> { tool });
    REQUIRE(countSubshapes(root, TopAbs_FACE) == 2);
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 3);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
}

TEST_CASE("GeometryTopologyEditor finds and repairs free edge gaps")
{
    const TopoDS_Face first = TopoDS::Face(
        GeometryBuilder::makeRectangleFace(
            0.0, 0.0, 0.0, 10.0, 5.0, CoordinatePlane::XY));
    const TopoDS_Face second = TopoDS::Face(
        GeometryBuilder::makeRectangleFace(
            10.005, 0.0, 0.0, 10.0, 5.0, CoordinatePlane::XY));

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, first);
    builder.Add(compound, second);
    const TopoDS_Shape root = makeGeometryRoot(compound);

    const std::vector<GeometryStitchCandidate> candidates =
        GeometryTopologyEditor::findStitchCandidates(root, 0.01);
    REQUIRE(candidates.size() == 1);
    REQUIRE(candidates.front().maximum_gap == Catch::Approx(0.005).margin(1.0e-6));

    const GeometryGapRepairResult result =
        GeometryTopologyEditor::repairFreeEdgeGaps(root, 0.01);
    REQUIRE(result.candidate_count == 1);
    REQUIRE(result.stitched_edge_count >= 1);
    REQUIRE(countSubshapes(result.shape, TopAbs_EDGE) < countSubshapes(root, TopAbs_EDGE));
    REQUIRE(BRepCheck_Analyzer(result.shape).IsValid());
}

TEST_CASE("GeometryTopologyEditor stitches two free boundary edges within tolerance")
{
    const TopoDS_Face left = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const TopoDS_Face right = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        10.005, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const TopoDS_Edge left_boundary = findEdgeOnX(left, 10.0);
    const TopoDS_Edge right_boundary = findEdgeOnX(right, 10.005);
    REQUIRE_FALSE(left_boundary.IsNull());
    REQUIRE_FALSE(right_boundary.IsNull());
    const TopoDS_Shape root = makeShapePairRoot(left, right);

    const TopoDS_Shape result = GeometryTopologyEditor::stitchBoundaryEdges(
        root, { left_boundary }, { right_boundary }, 0.01);

    REQUIRE(countSubshapes(result, TopAbs_FACE) == 2);
    REQUIRE(countSharedEdges(result) == 1);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
}

TEST_CASE("GeometryTopologyEditor stitches one long edge to two short edges")
{
    const TopoDS_Face left = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const auto [right, right_boundaries]
        = makeSplitLeftRectangleFace(10.005, 0.0, 10.0, 10.0);
    const TopoDS_Edge left_boundary = findEdgeOnX(left, 10.0);
    REQUIRE_FALSE(left_boundary.IsNull());
    REQUIRE(right_boundaries.size() == 2);
    const TopoDS_Shape root = makeShapePairRoot(left, right);

    const TopoDS_Shape result = GeometryTopologyEditor::stitchBoundaryEdges(
        root, { left_boundary }, right_boundaries, 0.01);

    REQUIRE(countSubshapes(result, TopAbs_FACE) == 2);
    REQUIRE(countSharedEdges(result) == 2);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
}

TEST_CASE("GeometryTopologyEditor rejects boundary edges outside stitch tolerance")
{
    const TopoDS_Face left = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const TopoDS_Face right = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        10.005, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const TopoDS_Edge left_boundary = findEdgeOnX(left, 10.0);
    const TopoDS_Edge right_boundary = findEdgeOnX(right, 10.005);
    const TopoDS_Shape root = makeShapePairRoot(left, right);

    REQUIRE_THROWS_AS(
        GeometryTopologyEditor::stitchBoundaryEdges(
            root, { left_boundary }, { right_boundary }, 0.001),
        std::runtime_error);
}

TEST_CASE("GeometryTopologyEditor expands stitchable free chain from a gap seed edge")
{
    const TopoDS_Face left = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const TopoDS_Face right = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        10.005, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const TopoDS_Edge left_boundary = findEdgeOnX(left, 10.0);
    REQUIRE_FALSE(left_boundary.IsNull());
    const TopoDS_Shape root = makeShapePairRoot(left, right);

    const std::vector<TopoDS_Edge> chain =
        GeometryTopologyEditor::expandStitchableFreeChain(root, left_boundary, 0.01);
    REQUIRE(chain.size() == 1);
    REQUIRE(chain.front().IsSame(left_boundary));
}

TEST_CASE("GeometryTopologyEditor finds the smallest gap partner chain and stitches from seed")
{
    const TopoDS_Face left = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const TopoDS_Face right = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        10.005, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const TopoDS_Edge left_boundary = findEdgeOnX(left, 10.0);
    REQUIRE_FALSE(left_boundary.IsNull());
    const TopoDS_Shape root = makeShapePairRoot(left, right);

    const std::vector<TopoDS_Edge> chain =
        GeometryTopologyEditor::expandStitchableFreeChain(root, left_boundary, 0.01);
    const std::vector<GeometryGapPartnerChain> partners =
        GeometryTopologyEditor::findGapPartnerChains(root, chain, 0.01);
    REQUIRE(partners.size() == 1);
    REQUIRE(partners.front().edges.size() == 1);
    REQUIRE(partners.front().maximum_gap == Catch::Approx(0.005).margin(1.0e-6));

    const TopoDS_Shape result =
        GeometryTopologyEditor::stitchGapFromSeedEdge(root, left_boundary, 0.01);
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 2);
    REQUIRE(countSharedEdges(result) == 1);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
}

TEST_CASE("GeometryTopologyEditor stitches a long gap seed edge to two short partners")
{
    const TopoDS_Face left = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const auto [right, right_boundaries]
        = makeSplitLeftRectangleFace(10.005, 0.0, 10.0, 10.0);
    const TopoDS_Edge left_boundary = findEdgeOnX(left, 10.0);
    REQUIRE_FALSE(left_boundary.IsNull());
    REQUIRE(right_boundaries.size() == 2);
    const TopoDS_Shape root = makeShapePairRoot(left, right);

    const TopoDS_Shape result =
        GeometryTopologyEditor::stitchGapFromSeedEdge(root, left_boundary, 0.01);
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 2);
    REQUIRE(countSharedEdges(result) == 2);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
}

TEST_CASE("GeometryTopologyEditor rejects a gap seed without a partner within tolerance")
{
    const TopoDS_Face left = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const TopoDS_Face right = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        10.005, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const TopoDS_Edge left_boundary = findEdgeOnX(left, 10.0);
    const TopoDS_Shape root = makeShapePairRoot(left, right);

    REQUIRE_THROWS_AS(
        GeometryTopologyEditor::stitchGapFromSeedEdge(root, left_boundary, 0.001),
        std::runtime_error);
}

TEST_CASE("GeometryTopologyEditor stitches free edges that share vertices but not topology")
{
    // 两面「共边但拓扑未共享」：同一对端点上各有一条独立 Edge，共享顶点却不共享边。
    const TopoDS_Vertex bottom = TopoDS::Vertex(GeometryBuilder::makePoint(0.0, 0.0, 0.0));
    const TopoDS_Vertex top = TopoDS::Vertex(GeometryBuilder::makePoint(0.0, 10.0, 0.0));
    const TopoDS_Edge left_boundary = TopoDS::Edge(GeometryBuilder::makeLine(bottom, top));
    const TopoDS_Edge right_boundary = TopoDS::Edge(GeometryBuilder::makeLine(bottom, top));

    BRepBuilderAPI_MakeWire left_wire;
    left_wire.Add(left_boundary);
    left_wire.Add(TopoDS::Edge(GeometryBuilder::makeLine(
        top, TopoDS::Vertex(GeometryBuilder::makePoint(-10.0, 10.0, 0.0)))));
    left_wire.Add(TopoDS::Edge(GeometryBuilder::makeLine(
        TopoDS::Vertex(GeometryBuilder::makePoint(-10.0, 10.0, 0.0)),
        TopoDS::Vertex(GeometryBuilder::makePoint(-10.0, 0.0, 0.0)))));
    left_wire.Add(TopoDS::Edge(GeometryBuilder::makeLine(
        TopoDS::Vertex(GeometryBuilder::makePoint(-10.0, 0.0, 0.0)), bottom)));
    REQUIRE(left_wire.IsDone());

    BRepBuilderAPI_MakeWire right_wire;
    right_wire.Add(right_boundary);
    right_wire.Add(TopoDS::Edge(GeometryBuilder::makeLine(
        top, TopoDS::Vertex(GeometryBuilder::makePoint(10.0, 10.0, 0.0)))));
    right_wire.Add(TopoDS::Edge(GeometryBuilder::makeLine(
        TopoDS::Vertex(GeometryBuilder::makePoint(10.0, 10.0, 0.0)),
        TopoDS::Vertex(GeometryBuilder::makePoint(10.0, 0.0, 0.0)))));
    right_wire.Add(TopoDS::Edge(GeometryBuilder::makeLine(
        TopoDS::Vertex(GeometryBuilder::makePoint(10.0, 0.0, 0.0)), bottom)));
    REQUIRE(right_wire.IsDone());

    const TopoDS_Face left = BRepBuilderAPI_MakeFace(left_wire.Wire()).Face();
    const TopoDS_Face right = BRepBuilderAPI_MakeFace(right_wire.Wire()).Face();
    REQUIRE_FALSE(left_boundary.IsSame(right_boundary));
    const TopoDS_Shape root = makeShapePairRoot(left, right);

    const TopoDS_Shape result =
        GeometryTopologyEditor::stitchGapFromSeedEdge(root, left_boundary, 0.01);
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 2);
    REQUIRE(countSharedEdges(result) >= 1);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
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

//! @brief 反向边按选择方向分割，同时保持所在线框的端点与拓扑有效性。
TEST_CASE("GeometryTopologyEditor splits a reversed edge along its orientation")
{
    const TopoDS_Edge edge = TopoDS::Edge(
        GeometryBuilder::makeLine(0.0, 0.0, 0.0, 10.0, 0.0, 0.0).Reversed());
    BRepBuilderAPI_MakeWire wire_builder(edge);
    const TopoDS_Shape result = GeometryTopologyEditor::splitEdge(
        makeGeometryRoot(wire_builder.Wire()), edge, 0.25);

    REQUIRE(BRepCheck_Analyzer(result).IsValid());
    REQUIRE(countSubshapes(result, TopAbs_EDGE) == 2);
    REQUIRE(countSubshapes(result, TopAbs_VERTEX) == 3);
    bool found_split_point = false;
    for (TopExp_Explorer it(result, TopAbs_VERTEX); it.More(); it.Next()) {
        if (BRep_Tool::Pnt(TopoDS::Vertex(it.Current())).Distance(gp_Pnt(7.5, 0.0, 0.0)) < 1.0e-7)
            found_split_point = true;
    }
    REQUIRE(found_split_point);
    TopExp_Explorer wire(result, TopAbs_WIRE);
    REQUIRE(wire.More());
    TopoDS_Vertex start;
    TopoDS_Vertex end;
    TopExp::Vertices(TopoDS::Wire(wire.Current()), start, end);
    REQUIRE(BRep_Tool::Pnt(start).Distance(gp_Pnt(10.0, 0.0, 0.0)) < 1.0e-7);
    REQUIRE(BRep_Tool::Pnt(end).Distance(gp_Pnt(0.0, 0.0, 0.0)) < 1.0e-7);
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

TEST_CASE("GeometryTopologyEditor preserves curved adjacency and recommends its endpoint")
{
    BRepBuilderAPI_MakeEdge curve_builder(
        gp_Circ(gp_Ax2(gp_Pnt(0.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0)), 10.0),
        0.0, 1.5707963267948966);
    REQUIRE(curve_builder.IsDone());
    const TopoDS_Edge curve_edge = curve_builder.Edge();
    TopoDS_Vertex curve_start;
    TopoDS_Vertex curve_end;
    TopExp::Vertices(curve_edge, curve_start, curve_end, true);
    const TopoDS_Vertex other_endpoint = TopoDS::Vertex(
        GeometryBuilder::makePoint(10.0, 1.0, 0.0));
    const TopoDS_Edge short_edge = TopoDS::Edge(
        GeometryBuilder::makeLine(curve_start, other_endpoint));
    const TopoDS_Edge straight_edge = TopoDS::Edge(GeometryBuilder::makeLine(
        other_endpoint,
        TopoDS::Vertex(GeometryBuilder::makePoint(20.0, 1.0, 0.0))));

    BRep_Builder builder;
    TopoDS_Compound shapes;
    builder.MakeCompound(shapes);
    builder.Add(shapes, curve_edge);
    builder.Add(shapes, short_edge);
    builder.Add(shapes, straight_edge);
    const TopoDS_Shape root = makeGeometryRoot(shapes);

    const TopoDS_Vertex recommended =
        GeometryTopologyEditor::recommendCollapseVertex(root, short_edge);
    REQUIRE(recommended.IsSame(curve_start));
    const TopoDS_Shape result = GeometryTopologyEditor::collapseEdge(
        root, short_edge, BRep_Tool::Pnt(recommended));
    REQUIRE(countSubshapes(result, TopAbs_EDGE) == 2);
    REQUIRE(countSubshapes(result, TopAbs_VERTEX) == 3);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
}

TEST_CASE("GeometryTopologyEditor merges independent vertices to the first vertex")
{
    const TopoDS_Vertex first = TopoDS::Vertex(GeometryBuilder::makePoint(0.0, 0.0, 0.0));
    const TopoDS_Vertex second = TopoDS::Vertex(GeometryBuilder::makePoint(1.0, 0.0, 0.0));
    BRep_Builder builder;
    TopoDS_Compound shapes;
    builder.MakeCompound(shapes);
    builder.Add(shapes, first);
    builder.Add(shapes, second);
    const TopoDS_Shape root = makeGeometryRoot(shapes);

    const TopoDS_Shape result = GeometryTopologyEditor::mergeVertices(
        root, { first, second }, gp_Pnt(0.0, 0.0, 0.0));
    REQUIRE(countSubshapes(result, TopAbs_VERTEX) == 1);
    TopExp_Explorer vertex_exp(result, TopAbs_VERTEX);
    REQUIRE(BRep_Tool::Pnt(TopoDS::Vertex(vertex_exp.Current()))
                .Distance(gp_Pnt(0.0, 0.0, 0.0))
        < 1.0e-7);
}

//! @brief 合并孤立边的端点后保留目标点，兼顾根中仍有其他独立几何的情况。
TEST_CASE("GeometryTopologyEditor merges endpoints of an isolated edge")
{
    const TopoDS_Edge edge = TopoDS::Edge(
        GeometryBuilder::makeLine(0.0, 0.0, 0.0, 0.005, 0.0, 0.0));
    const TopoDS_Edge other = TopoDS::Edge(
        GeometryBuilder::makeLine(10.0, 0.0, 0.0, 20.0, 0.0, 0.0));
    TopoDS_Vertex first;
    TopoDS_Vertex second;
    TopExp::Vertices(edge, first, second);
    for (bool keep_other : { false, true }) {
        CAPTURE(keep_other);
        BRep_Builder builder;
        TopoDS_Compound root;
        builder.MakeCompound(root);
        builder.Add(root, edge);
        if (keep_other)
            builder.Add(root, other);
        const TopoDS_Shape result = GeometryTopologyEditor::mergeVertices(
            root, { first, second }, gp_Pnt(0.0025, 0.0, 0.0));
        REQUIRE_FALSE(result.IsNull());
        REQUIRE(BRepCheck_Analyzer(result).IsValid());
        REQUIRE(countSubshapes(result, TopAbs_EDGE) == (keep_other ? 1 : 0));
        REQUIRE(countSubshapes(result, TopAbs_VERTEX) == (keep_other ? 3 : 1));
        bool found_destination = false;
        for (TopExp_Explorer it(result, TopAbs_VERTEX); it.More(); it.Next()) {
            if (BRep_Tool::Pnt(TopoDS::Vertex(it.Current())).Distance(gp_Pnt(0.0025, 0.0, 0.0)) < 1.0e-7)
                found_destination = true;
        }
        REQUIRE(found_destination);
    }
}

TEST_CASE("GeometryTopologyEditor merges selected same-domain faces")
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

    // 模拟真实插件流程：对角线既是分割工具，也是根节点下的独立几何 Edge。
    BRep_Builder builder;
    TopoDS_Compound shapes;
    builder.MakeCompound(shapes);
    builder.Add(shapes, box);
    builder.Add(shapes, diagonal);
    const TopoDS_Shape root = makeGeometryRoot(shapes);
    const TopoDS_Shape split_result = GeometryTopologyEditor::splitFace(
        root, bottom_face, std::vector<TopoDS_Edge> { diagonal });
    const std::vector<TopoDS_Face> split_faces = findFacesOnZPlane(split_result, 0.0);
    REQUIRE(split_faces.size() == 2);
    REQUIRE(countSubshapes(split_result, TopAbs_EDGE) == 13);

    const TopoDS_Shape result = GeometryTopologyEditor::mergeFaces(split_result, split_faces);
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 6);
    REQUIRE(countSubshapes(result, TopAbs_EDGE) == 12);
    const std::vector<TopoDS_Face> merged_faces = findFacesOnZPlane(result, 0.0);
    REQUIRE(merged_faces.size() == 1);
    REQUIRE(countSubshapes(merged_faces.front(), TopAbs_EDGE) == 4);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
}

TEST_CASE("GeometryTopologyEditor rejects incomplete face merge inputs")
{
    const TopoDS_Face face = TopoDS::Face(
        GeometryBuilder::makeRectangleFace(
            0.0, 0.0, 0.0, 10.0, 20.0, CoordinatePlane::XY));
    const TopoDS_Shape root = makeGeometryRoot(face);

    REQUIRE_THROWS_AS(
        GeometryTopologyEditor::mergeFaces(root, { face }),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        GeometryTopologyEditor::mergeFaces(root, { face, face }),
        std::invalid_argument);
}

TEST_CASE("GeometryTopologyEditor merges selected same-domain edges")
{
    const TopoDS_Vertex first = TopoDS::Vertex(GeometryBuilder::makePoint(0.0, 0.0, 0.0));
    const TopoDS_Vertex middle = TopoDS::Vertex(GeometryBuilder::makePoint(10.0, 0.0, 0.0));
    const TopoDS_Vertex last = TopoDS::Vertex(GeometryBuilder::makePoint(20.0, 0.0, 0.0));
    const TopoDS_Edge first_edge = TopoDS::Edge(GeometryBuilder::makeLine(first, middle));
    const TopoDS_Edge second_edge = TopoDS::Edge(GeometryBuilder::makeLine(middle, last));

    BRepBuilderAPI_MakeWire wire_builder;
    wire_builder.Add(first_edge);
    wire_builder.Add(second_edge);
    REQUIRE(wire_builder.IsDone());
    const TopoDS_Shape root = makeGeometryRoot(wire_builder.Wire());

    const TopoDS_Shape result = GeometryTopologyEditor::mergeEdges(
        root, std::vector<TopoDS_Edge> { first_edge, second_edge });
    REQUIRE(countSubshapes(result, TopAbs_EDGE) == 1);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
}

TEST_CASE("GeometryTopologyEditor rejects incomplete edge merge inputs")
{
    const TopoDS_Edge edge = TopoDS::Edge(
        GeometryBuilder::makeLine(0.0, 0.0, 0.0, 10.0, 0.0, 0.0));
    const TopoDS_Shape root = makeGeometryRoot(edge);

    REQUIRE_THROWS_AS(
        GeometryTopologyEditor::mergeEdges(root, { edge }),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        GeometryTopologyEditor::mergeEdges(root, { edge, edge }),
        std::invalid_argument);
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

TEST_CASE("GeometryTopologyEditor removes a nested face from a solid")
{
    const TopoDS_Shape box =
        GeometryBuilder::makeBox(0.0, 0.0, 0.0, 10.0, 20.0, 30.0);
    TopExp_Explorer face_exp(box, TopAbs_FACE);
    REQUIRE(face_exp.More());
    const TopoDS_Face nested_face = TopoDS::Face(face_exp.Current());
    const TopoDS_Shape root = makeGeometryRoot(box);
    REQUIRE(countSubshapes(root, TopAbs_FACE) == 6);

    const TopoDS_Shape result =
        GeometryTopologyEditor::removeShape(root, nested_face, true);

    REQUIRE_FALSE(result.IsNull());
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 5);
    // 缺面后 Solid 不再闭合，结果应为 Shell 而非无效 Solid。
    REQUIRE(countSubshapes(result, TopAbs_SOLID) == 0);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
}

TEST_CASE("GeometryTopologyEditor removes a nested face from an open shell")
{
    // 开口 Shell（无 Solid 包装），与 STEP 自由面组结构一致。
    const TopoDS_Face bottom = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const TopoDS_Face left = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XZ));
    BRep_Builder builder;
    TopoDS_Shell shell;
    builder.MakeShell(shell);
    builder.Add(shell, bottom);
    builder.Add(shell, left);
    shell.Closed(false);
    const TopoDS_Shape root = makeGeometryRoot(shell);
    REQUIRE(countSubshapes(root, TopAbs_FACE) == 2);
    REQUIRE(countSubshapes(root, TopAbs_SHELL) == 1);

    const TopoDS_Shape result =
        GeometryTopologyEditor::removeShape(root, bottom, true);

    REQUIRE_FALSE(result.IsNull());
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 1);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
}

TEST_CASE("GeometryTopologyEditor deletes the only face of a single-face shell")
{
    // 突出小面片常是「单面壳」：删掉后整块几何为空，应允许。
    const TopoDS_Face face = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 5.0, 5.0, CoordinatePlane::XY));
    BRep_Builder builder;
    TopoDS_Shell shell;
    builder.MakeShell(shell);
    builder.Add(shell, face);
    shell.Closed(false);
    const TopoDS_Shape root = makeGeometryRoot(shell);
    REQUIRE(countSubshapes(root, TopAbs_FACE) == 1);

    const TopoDS_Shape result =
        GeometryTopologyEditor::removeShape(root, face, true);
    REQUIRE(result.IsNull());
}

//! @brief 删除最后一个壳面时，根中的独立边与点仍应保留原有身份。
TEST_CASE("GeometryTopologyEditor preserves independent geometry after deleting the last face")
{
    const TopoDS_Face face = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 5.0, 5.0, CoordinatePlane::XY));
    const TopoDS_Shape edge = GeometryBuilder::makeLine(10.0, 0.0, 0.0, 20.0, 0.0, 0.0);
    const TopoDS_Shape point = GeometryBuilder::makePoint(30.0, 0.0, 0.0);
    BRep_Builder builder;
    TopoDS_Shell shell;
    builder.MakeShell(shell);
    builder.Add(shell, face);
    TopoDS_Compound root;
    builder.MakeCompound(root);
    builder.Add(root, shell);
    builder.Add(root, edge);
    builder.Add(root, point);

    const TopoDS_Shape result = GeometryTopologyEditor::removeShape(root, face, true);
    REQUIRE_FALSE(result.IsNull());
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 0);
    REQUIRE(countSubshapes(result, TopAbs_EDGE) == 1);
    REQUIRE(countSubshapes(result, TopAbs_VERTEX) == 3);
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> shapes;
    TopExp::MapShapes(result, shapes);
    REQUIRE(shapes.Contains(edge));
    REQUIRE(shapes.Contains(point));
}

TEST_CASE("GeometryTopologyEditor removes a face nested under a compound wrapper in a shell")
{
    // 模拟 BRepFeat 分割后：Shell 子节点里挂着 Compound[两块面]。
    const TopoDS_Shape box =
        GeometryBuilder::makeBox(0.0, 0.0, 0.0, 10.0, 20.0, 30.0);
    const TopoDS_Face bottom_face = findFaceOnZPlane(box, 0.0);
    REQUIRE_FALSE(bottom_face.IsNull());
    const TopoDS_Vertex first = findFaceVertex(bottom_face, 0.0, 0.0, 0.0);
    const TopoDS_Vertex second = findFaceVertex(bottom_face, 10.0, 20.0, 0.0);
    const TopoDS_Edge diagonal = TopoDS::Edge(GeometryBuilder::makeLine(first, second));
    const TopoDS_Shape split_result = GeometryTopologyEditor::splitFace(
        makeGeometryRoot(box), bottom_face, std::vector<TopoDS_Edge> { diagonal });
    REQUIRE(countSubshapes(split_result, TopAbs_FACE) == 7);

    // 把分割产生的底面两块重新包进 Compound，塞回原 Shell 位置（模拟包装残留）。
    std::vector<TopoDS_Face> bottom_pieces;
    for (TopExp_Explorer face_exp(split_result, TopAbs_FACE); face_exp.More(); face_exp.Next()) {
        const TopoDS_Face face = TopoDS::Face(face_exp.Current());
        bool on_z0 = true;
        for (TopExp_Explorer vertex_exp(face, TopAbs_VERTEX); vertex_exp.More(); vertex_exp.Next()) {
            if (std::abs(BRep_Tool::Pnt(TopoDS::Vertex(vertex_exp.Current())).Z()) > 1.0e-7) {
                on_z0 = false;
                break;
            }
        }
        if (on_z0)
            bottom_pieces.push_back(face);
    }
    REQUIRE(bottom_pieces.size() == 2);

    const TopoDS_Shape result =
        GeometryTopologyEditor::removeShape(split_result, bottom_pieces.front(), true);
    REQUIRE_FALSE(result.IsNull());
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 6);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
}

TEST_CASE("GeometryTopologyEditor removes a split face piece from a solid")
{
    // 模拟「分割突出面片后删除小块」工作流：先切开底面，再删掉其中一块。
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

    const TopoDS_Shape split_result = GeometryTopologyEditor::splitFace(
        root, bottom_face, std::vector<TopoDS_Edge> { diagonal });
    REQUIRE(countSubshapes(split_result, TopAbs_FACE) == 7);

    // 取分割后两块底面中面积较小的一块作为「突出面片」删除。
    std::vector<TopoDS_Face> bottom_pieces;
    for (TopExp_Explorer face_exp(split_result, TopAbs_FACE); face_exp.More(); face_exp.Next()) {
        const TopoDS_Face face = TopoDS::Face(face_exp.Current());
        bool on_z0 = true;
        for (TopExp_Explorer vertex_exp(face, TopAbs_VERTEX); vertex_exp.More(); vertex_exp.Next()) {
            if (std::abs(BRep_Tool::Pnt(TopoDS::Vertex(vertex_exp.Current())).Z()) > 1.0e-7) {
                on_z0 = false;
                break;
            }
        }
        if (on_z0)
            bottom_pieces.push_back(face);
    }
    REQUIRE(bottom_pieces.size() == 2);

    const TopoDS_Shape result =
        GeometryTopologyEditor::removeShape(split_result, bottom_pieces.front(), true);

    REQUIRE_FALSE(result.IsNull());
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 6);
    REQUIRE(countSubshapes(result, TopAbs_SOLID) == 0);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
}

TEST_CASE("GeometryTopologyEditor rejects deleting a nested edge alone")
{
    const TopoDS_Shape box =
        GeometryBuilder::makeBox(0.0, 0.0, 0.0, 10.0, 20.0, 30.0);
    TopExp_Explorer edge_exp(box, TopAbs_EDGE);
    REQUIRE(edge_exp.More());
    const TopoDS_Edge nested_edge = TopoDS::Edge(edge_exp.Current());
    const TopoDS_Shape root = makeGeometryRoot(box);

    REQUIRE_THROWS_AS(
        GeometryTopologyEditor::removeShape(root, nested_edge, true),
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

//! @brief 嵌套面非级联删除应保留独占边，级联删除仍清理独占边且不触碰其他几何。
TEST_CASE("GeometryTopologyEditor preserves nested face children when requested")
{
    const TopoDS_Face face = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 5.0, 5.0, CoordinatePlane::XY));
    const TopoDS_Shape other = GeometryBuilder::makeLine(10.0, 0.0, 0.0, 20.0, 0.0, 0.0);
    BRep_Builder builder;
    TopoDS_Shell shell;
    builder.MakeShell(shell);
    builder.Add(shell, face);
    TopoDS_Compound root;
    builder.MakeCompound(root);
    builder.Add(root, shell);
    builder.Add(root, other);
    for (bool cascade : { false, true }) {
        CAPTURE(cascade);
        const auto result = GeometryTopologyEditor::removeShape(root, face, cascade);
        REQUIRE_FALSE(result.IsNull());
        REQUIRE(BRepCheck_Analyzer(result).IsValid());
        REQUIRE(countSubshapes(result, TopAbs_FACE) == 0);
        REQUIRE(countSubshapes(result, TopAbs_EDGE) == (cascade ? 1 : 5));
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edges;
        TopExp::MapShapes(result, TopAbs_EDGE, edges);
        REQUIRE(edges.Contains(other));
        for (TopExp_Explorer edge(face, TopAbs_EDGE); edge.More(); edge.Next())
            REQUIRE(edges.Contains(edge.Current()) == !cascade);
    }
    // 没有其他形状时也要把整个边界留下，而不是返回空 Shape。
    const auto edges_only = GeometryTopologyEditor::removeShape(makeGeometryRoot(shell), face, false);
    REQUIRE_FALSE(edges_only.IsNull());
    REQUIRE(countSubshapes(edges_only, TopAbs_EDGE) == 4);
}

//! @brief 共面孔洞新增一个面并共享全部边界，原始开口形状不被原地改变。
TEST_CASE("GeometryTopologyEditor fills a planar hole")
{
    const auto box = GeometryBuilder::makeBox(0.0, 0.0, 0.0, 10.0, 10.0, 10.0);
    const auto face = findFaceOnZPlane(box, 10.0);
    const auto open = GeometryTopologyEditor::removeShape(makeGeometryRoot(box), face, true);
    const auto seed = TopoDS::Edge(TopExp_Explorer(face, TopAbs_EDGE).Current());
    const auto result = GeometryTopologyEditor::fillBoundaryLoop(open, seed);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 6);
    REQUIRE(countSharedEdges(result) == 12);
    REQUIRE(countSubshapes(open, TopAbs_FACE) == 5);
    REQUIRE(countSharedEdges(open) == 8);
}

//! @brief 四个三角侧面围出非共面孔洞，验证曲面拟合而非平面限定。
TEST_CASE("GeometryTopologyEditor fills a nonplanar hole")
{
    std::vector<TopoDS_Vertex> corners;
    for (const gp_Pnt& point : { gp_Pnt(0, 0, 0), gp_Pnt(10, 0, 0),
             gp_Pnt(10, 10, 2), gp_Pnt(0, 10, 0) })
        corners.push_back(TopoDS::Vertex(GeometryBuilder::makePoint(point.X(), point.Y(), point.Z())));
    const auto apex = TopoDS::Vertex(GeometryBuilder::makePoint(5, 5, -10));
    std::vector<TopoDS_Edge> radials;
    std::vector<TopoDS_Edge> rim;
    for (size_t index = 0; index < corners.size(); ++index) {
        radials.push_back(BRepBuilderAPI_MakeEdge(apex, corners[index]).Edge());
        rim.push_back(BRepBuilderAPI_MakeEdge(corners[index], corners[(index + 1) % corners.size()]).Edge());
    }
    BRep_Builder builder;
    TopoDS_Shell shell;
    builder.MakeShell(shell);
    for (size_t index = 0; index < corners.size(); ++index) {
        BRepBuilderAPI_MakeWire wire;
        wire.Add(radials[index]);
        wire.Add(rim[index]);
        wire.Add(TopoDS::Edge(radials[(index + 1) % corners.size()].Reversed()));
        builder.Add(shell, BRepBuilderAPI_MakeFace(wire.Wire()).Face());
    }
    REQUIRE(BRepCheck_Analyzer(shell).IsValid());
    const auto result = GeometryTopologyEditor::fillBoundaryLoop(makeGeometryRoot(shell), rim[0]);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 5);
    REQUIRE(countSharedEdges(result) == 8);
    REQUIRE(countSharedEdges(shell) == 4);
}

//! @brief 拒绝把单个面的外边界当作孔洞补出重叠面，失败不改变输入。
TEST_CASE("GeometryTopologyEditor rejects filling an existing face outline")
{
    const auto face = TopoDS::Face(GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 10.0, CoordinatePlane::XY));
    const auto root = makeGeometryRoot(face);
    REQUIRE_THROWS_AS(GeometryTopologyEditor::fillBoundaryLoop(root,
                          findEdgeOnX(face, 0.0)),
        std::runtime_error);
    REQUIRE(countSubshapes(root, TopAbs_FACE) == 1);
    REQUIRE(BRepCheck_Analyzer(root).IsValid());
}

//! @brief 从同一原始 Shell 取出相邻两面，非级联删除只提升独占边，不重复共享边。
TEST_CASE("GeometryTopologyEditor keeps exclusive edges of a face in a multi-face shell")
{
    const auto box = GeometryBuilder::makeBox(0.0, 0.0, 0.0, 10.0, 10.0, 10.0);
    const auto bottom = findFaceOnZPlane(box, 0.0);
    const auto bottom_edge = TopExp_Explorer(bottom, TopAbs_EDGE).Current();
    NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
        TopTools_ShapeMapHasher>
        edge_faces;
    TopExp::MapShapesAndUniqueAncestors(box, TopAbs_EDGE, TopAbs_FACE, edge_faces);
    TopoDS_Shape neighbor;
    for (const auto& candidate : edge_faces.FindFromKey(bottom_edge)) {
        if (!candidate.IsSame(bottom))
            neighbor = candidate;
    }
    REQUIRE_FALSE(neighbor.IsNull());
    BRep_Builder builder;
    TopoDS_Shell shell;
    builder.MakeShell(shell);
    builder.Add(shell, bottom);
    builder.Add(shell, neighbor);
    const auto result = GeometryTopologyEditor::removeShape(makeGeometryRoot(shell), bottom, false);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 1);
    REQUIRE(countSubshapes(result, TopAbs_EDGE) == 7);
}
//! @brief 整圆只有一条闭合 Edge，不能被误判为无端点或分支边界。
TEST_CASE("GeometryTopologyEditor fills a circular hole")
{
    const auto cylinder = GeometryBuilder::makeCylinder(0, 0, 0, 5, 10, 0, 0, 1, 2 * std::acos(-1.0));
    const auto cap = findFaceOnZPlane(cylinder, 10.0);
    REQUIRE_FALSE(cap.IsNull());
    const auto open = GeometryTopologyEditor::removeShape(makeGeometryRoot(cylinder), cap, true);
    const auto seed = TopoDS::Edge(TopExp_Explorer(cap, TopAbs_EDGE).Current());
    const auto result = GeometryTopologyEditor::fillBoundaryLoop(open, seed);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 3);
    REQUIRE(countSharedEdges(result) == 2);
}

//! @brief 分支上找到的最小环若已被现有面覆盖，应拒绝重复盖面。
TEST_CASE("GeometryTopologyEditor rejects an occupied minimum boundary loop")
{
    const auto center = TopoDS::Vertex(GeometryBuilder::makePoint(0, 0, 0));
    BRep_Builder builder;
    TopoDS_Compound root;
    builder.MakeCompound(root);
    TopoDS_Edge seed;
    for (double sign : { -1.0, 1.0 }) {
        const auto a = TopoDS::Vertex(GeometryBuilder::makePoint(sign * 10, 0, 0));
        const auto b = TopoDS::Vertex(GeometryBuilder::makePoint(0, sign * 10, 0));
        BRepBuilderAPI_MakeWire wire;
        const auto edge = BRepBuilderAPI_MakeEdge(center, a).Edge();
        wire.Add(edge);
        wire.Add(BRepBuilderAPI_MakeEdge(a, b).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(b, center).Edge());
        builder.Add(root, BRepBuilderAPI_MakeFace(wire.Wire()).Face());
        seed = edge;
    }
    REQUIRE_THROWS_AS(GeometryTopologyEditor::fillBoundaryLoop(root, seed), std::runtime_error);
    REQUIRE(countSubshapes(root, TopAbs_FACE) == 2);
}

//! @brief 无论从哪一侧选择，公共边都准确落在对侧；长短边分段不影响方向。
TEST_CASE("GeometryTopologyEditor stitches the selected side onto the stationary side")
{
    for (bool reverse : { false, true }) {
        CAPTURE(reverse);
        const auto left = TopoDS::Face(GeometryBuilder::makeRectangleFace(0, 0, 0, 10, 10, CoordinatePlane::XY));
        const auto [right, right_edges] = makeSplitLeftRectangleFace(10.005, 0, 10, 10);
        const auto left_edge = findEdgeOnX(left, 10);
        const auto root = makeShapePairRoot(left, right);
        const auto result = GeometryTopologyEditor::stitchGapFromSeedEdge(
            root, reverse ? right_edges.front() : left_edge, 0.01);
        REQUIRE(countSharedEdges(result) == (reverse ? 1 : 2));
        NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher> edge_faces;
        TopExp::MapShapesAndUniqueAncestors(result, TopAbs_EDGE, TopAbs_FACE, edge_faces);
        for (int index = 1; index <= edge_faces.Extent(); ++index) {
            if (edge_faces.FindFromIndex(index).Size() != 2)
                continue;
            BRepAdaptor_Curve curve(TopoDS::Edge(edge_faces.FindKey(index)));
            for (double ratio : { 0.0, 0.5, 1.0 }) {
                const auto point = curve.Value(curve.FirstParameter() + ratio * (curve.LastParameter() - curve.FirstParameter()));
                REQUIRE(point.X() == Catch::Approx(reverse ? 10.0 : 10.005).margin(1.e-7));
            }
        }
        REQUIRE(countSharedEdges(root) == 0);
        REQUIRE(BRepCheck_Analyzer(result).IsValid());
    }
}

//! @brief 用多个顶点分段的短路径与边数更少的长路径竞争，按总弧长补最小环。
TEST_CASE("GeometryTopologyEditor fills the shortest length loop through a branched boundary")
{
    std::vector<TopoDS_Vertex> vertices;
    for (const auto& point : { gp_Pnt(0, 0, 0), gp_Pnt(10, 0, 0), gp_Pnt(10, 1, 0),
             gp_Pnt(5, 1, 0), gp_Pnt(0, 1, 0), gp_Pnt(5, 10, 0) })
        vertices.push_back(TopoDS::Vertex(GeometryBuilder::makePoint(point.X(), point.Y(), point.Z())));
    BRep_Builder builder;
    TopoDS_Compound root;
    builder.MakeCompound(root);
    TopoDS_Edge seed;
    int face_index = 0;
    for (const auto& [a, b] : std::vector<std::pair<int, int>> { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 }, { 4, 0 }, { 1, 5 }, { 5, 0 } }) {
        const auto edge = BRepBuilderAPI_MakeEdge(vertices[a], vertices[b]).Edge();
        const auto apex = TopoDS::Vertex(GeometryBuilder::makePoint(50 + face_index * 31, -30 - face_index * 17, -100));
        BRepBuilderAPI_MakeWire wire;
        wire.Add(edge);
        wire.Add(BRepBuilderAPI_MakeEdge(vertices[b], apex).Edge());
        wire.Add(BRepBuilderAPI_MakeEdge(apex, vertices[a]).Edge());
        builder.Add(root, BRepBuilderAPI_MakeFace(wire.Wire()).Face());
        if (face_index++ == 0)
            seed = edge;
    }
    const auto result = GeometryTopologyEditor::fillBoundaryLoop(root, seed);
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 8);
    REQUIRE(countSharedEdges(result) == 5);
    bool found_patch = false;
    for (TopExp_Explorer face(result, TopAbs_FACE); face.More(); face.Next()) {
        if (!findFaceOnZPlane(face.Current(), 0).IsNull()) {
            GProp_GProps properties;
            BRepGProp::SurfaceProperties(face.Current(), properties);
            REQUIRE(properties.Mass() == Catch::Approx(10).margin(1.e-5));
            found_patch = true;
        }
    }
    REQUIRE(found_patch);
}

//! @brief 相距较远的点合并时，边的三维曲线端点随之移动，容差不随位移增大。
TEST_CASE("GeometryTopologyEditor deforms incident lines and curves for distant merged vertices")
{
    for (bool curved : { false, true }) {
        CAPTURE(curved);
        const auto edge = curved
            ? BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 10), 0, 1.0).Edge()
            : TopoDS::Edge(GeometryBuilder::makeLine(0, 0, 0, 10, 0, 0));
        const auto first = TopExp::FirstVertex(edge);
        const auto other = TopoDS::Vertex(GeometryBuilder::makePoint(20, 20, 5));
        const auto root = makeShapePairRoot(edge, other);
        const auto target = BRep_Tool::Pnt(other);
        const auto result = GeometryTopologyEditor::mergeVertices(root, { first, other }, target);
        const auto moved_edge = TopoDS::Edge(TopExp_Explorer(result, TopAbs_EDGE).Current());
        BRepAdaptor_Curve moved(moved_edge);
        REQUIRE(moved.Value(moved.FirstParameter()).Distance(target) < 1.e-6);
        REQUIRE(BRep_Tool::Tolerance(TopExp::FirstVertex(moved_edge)) < 1.e-5);
        REQUIRE(BRepCheck_Analyzer(result).IsValid());
        REQUIRE(BRep_Tool::Pnt(first).Distance(target) > 1);
    }
}

//! @brief 将面角点合并到平面外的独立点时，允许邻面变为曲面而保持完整边界。
TEST_CASE("GeometryTopologyEditor deforms an incident face for an out of plane vertex merge")
{
    const auto face = GeometryBuilder::makeRectangleFace(0, 0, 0, 10, 10, CoordinatePlane::XY);
    const auto vertex = TopoDS::Vertex(TopExp_Explorer(face, TopAbs_VERTEX).Current());
    const auto point = BRep_Tool::Pnt(vertex);
    const auto destination = TopoDS::Vertex(GeometryBuilder::makePoint(point.X(), point.Y(), 3));
    const auto root = makeShapePairRoot(face, destination);
    const auto result = GeometryTopologyEditor::mergeVertices(root, { vertex, destination }, BRep_Tool::Pnt(destination));
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 1);
    REQUIRE(countSubshapes(result, TopAbs_EDGE) == 4);
    REQUIRE(countSubshapes(result, TopAbs_VERTEX) == 4);
    REQUIRE(BRep_Tool::Pnt(vertex).Z() == 0);
}

//! @brief 两片面之间的开放间隙不能凭空加桥接端边，也不能覆盖原面。
TEST_CASE("GeometryTopologyEditor does not bridge open gaps when filling")
{
    const auto left = TopoDS::Face(GeometryBuilder::makeRectangleFace(0, 0, 0, 10, 10, CoordinatePlane::XY));
    const auto right = GeometryBuilder::makeRectangleFace(10.005, 0, 0, 10, 10, CoordinatePlane::XY);
    const auto root = makeShapePairRoot(left, right);
    REQUIRE_THROWS_AS(GeometryTopologyEditor::fillBoundaryLoop(root, findEdgeOnX(left, 10)), std::runtime_error);
    REQUIRE(countSubshapes(root, TopAbs_FACE) == 2);
}

//! @brief 实体角点移动需同时重建三张邻面，维持共享边和实体有效性。
TEST_CASE("GeometryTopologyEditor deforms all faces around a solid vertex")
{
    const auto box = GeometryBuilder::makeBox(0, 0, 0, 10, 10, 10);
    const auto vertex = TopoDS::Vertex(TopExp_Explorer(box, TopAbs_VERTEX).Current());
    const auto point = BRep_Tool::Pnt(vertex);
    const auto destination = TopoDS::Vertex(GeometryBuilder::makePoint(point.X() - 1, point.Y() - 1, point.Z() - 1));
    const auto root = makeShapePairRoot(box, destination);
    const auto result = GeometryTopologyEditor::mergeVertices(root, { vertex, destination }, BRep_Tool::Pnt(destination));
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
    REQUIRE(countSubshapes(result, TopAbs_SOLID) == 1);
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 6);
    REQUIRE(countSharedEdges(result) == 12);
    for (TopExp_Explorer v(result, TopAbs_VERTEX); v.More(); v.Next())
        REQUIRE(BRep_Tool::Tolerance(TopoDS::Vertex(v.Current())) < 1.e-5);
}

//! @brief 合并实体上一条边的两端点时移除退化边，仍保留封闭共享拓扑。
TEST_CASE("GeometryTopologyEditor merges adjacent solid vertices with geometric reconstruction")
{
    const auto box = GeometryBuilder::makeBox(0, 0, 0, 10, 10, 10);
    const auto edge = TopoDS::Edge(TopExp_Explorer(box, TopAbs_EDGE).Current());
    const auto first = TopExp::FirstVertex(edge);
    const auto last = TopExp::LastVertex(edge);
    const auto target = gp_Pnt((BRep_Tool::Pnt(first).XYZ() + BRep_Tool::Pnt(last).XYZ()) * 0.5);
    const auto result = GeometryTopologyEditor::mergeVertices(makeGeometryRoot(box), { first, last }, target);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
    REQUIRE(countSubshapes(result, TopAbs_VERTEX) == 7);
    REQUIRE(countSubshapes(result, TopAbs_EDGE) == 11);
    REQUIRE(countSharedEdges(result) == 11);
}

//! @brief 全部边都未邻接面时，仍可补平面或非共面闭环，并保留无关孤立边。
TEST_CASE("GeometryTopologyEditor fills isolated planar and nonplanar edge loops")
{
    for (double height : { 0.0, 2.0 }) {
        CAPTURE(height);
        std::vector<TopoDS_Vertex> vertices;
        for (const auto& point : { gp_Pnt(0, 0, 0), gp_Pnt(10, 0, 0), gp_Pnt(10, 10, height), gp_Pnt(0, 10, 0) })
            vertices.push_back(TopoDS::Vertex(GeometryBuilder::makePoint(point.X(), point.Y(), point.Z())));
        BRep_Builder builder;
        TopoDS_Compound root;
        builder.MakeCompound(root);
        std::vector<TopoDS_Edge> edges;
        for (size_t i = 0; i < vertices.size(); ++i) {
            edges.push_back(BRepBuilderAPI_MakeEdge(vertices[i], vertices[(i + 1) % vertices.size()]).Edge());
            builder.Add(root, edges.back());
        }
        builder.Add(root, GeometryBuilder::makeLine(20, 0, 0, 30, 0, 0));
        const auto result = GeometryTopologyEditor::fillBoundaryLoop(root, edges.front());
        REQUIRE(BRepCheck_Analyzer(result).IsValid());
        REQUIRE(countSubshapes(result, TopAbs_FACE) == 1);
        REQUIRE(countSubshapes(result, TopAbs_EDGE) == 5);
        REQUIRE(countSubshapes(result, TopAbs_VERTEX) == 6);
        REQUIRE(countSubshapes(root, TopAbs_FACE) == 0);
    }
}

//! @brief 孤立边与单面自由边共同围成的最短闭环，都可作为种子边。
TEST_CASE("GeometryTopologyEditor fills mixed isolated and face boundary loops")
{
    const auto face = TopoDS::Face(GeometryBuilder::makeRectangleFace(0, 0, 0, 10, 10, CoordinatePlane::XY));
    const auto boundary = findEdgeOnX(face, 10);
    const auto first = TopExp::FirstVertex(boundary, true);
    const auto last = TopExp::LastVertex(boundary, true);
    const auto tip = TopoDS::Vertex(GeometryBuilder::makePoint(11, 5, 0));
    const auto a = BRepBuilderAPI_MakeEdge(first, tip).Edge();
    const auto b = BRepBuilderAPI_MakeEdge(tip, last).Edge();
    BRep_Builder builder;
    TopoDS_Compound root;
    builder.MakeCompound(root);
    builder.Add(root, face);
    builder.Add(root, a);
    builder.Add(root, b);
    for (const auto& seed : { boundary, a }) {
        const auto result = GeometryTopologyEditor::fillBoundaryLoop(root, seed);
        REQUIRE(BRepCheck_Analyzer(result).IsValid());
        REQUIRE(countSubshapes(result, TopAbs_FACE) == 2);
        REQUIRE(countSubshapes(result, TopAbs_EDGE) == 6);
        REQUIRE(countSharedEdges(result) == 1);
    }
}

//! @brief 只有一条闭合圆边且不属于任何面时也应补面；开放孤立边链仍拒绝。
TEST_CASE("GeometryTopologyEditor fills isolated circles and rejects open isolated chains")
{
    const auto circle = BRepBuilderAPI_MakeEdge(gp_Circ(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 5)).Edge();
    const auto result = GeometryTopologyEditor::fillBoundaryLoop(makeGeometryRoot(circle), circle);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
    REQUIRE(countSubshapes(result, TopAbs_FACE) == 1);
    REQUIRE(countSubshapes(result, TopAbs_EDGE) == 1);
    const auto line = TopoDS::Edge(GeometryBuilder::makeLine(0, 0, 0, 10, 0, 0));
    REQUIRE_THROWS_AS(GeometryTopologyEditor::fillBoundaryLoop(makeGeometryRoot(line), line), std::runtime_error);
}

//! @brief 放宽最大间隙不能把窄面端部的短边吸入已能缝合的种子边链。
TEST_CASE("GeometryTopologyEditor keeps the nearest local stitch when the maximum gap grows")
{
    const auto left = TopoDS::Face(GeometryBuilder::makeRectangleFace(0, 0, 0, 0.05, 10, CoordinatePlane::XY));
    const auto right = TopoDS::Face(GeometryBuilder::makeRectangleFace(0.055, 0, 0, 10, 10, CoordinatePlane::XY));
    const auto seed = findEdgeOnX(left, 0.05);
    const auto root = makeShapePairRoot(left, right);
    for (double tolerance : { 0.01, 0.1, 1.0 }) {
        CAPTURE(tolerance);
        const auto result = GeometryTopologyEditor::stitchGapFromSeedEdge(root, seed, tolerance);
        REQUIRE(BRepCheck_Analyzer(result).IsValid());
        REQUIRE(countSharedEdges(result) == 1);
        REQUIRE(countSubshapes(result, TopAbs_FACE) == 2);
        REQUIRE(countSubshapes(result, TopAbs_EDGE) == 7);
    }
}

//! @brief 同一个窄面的其他边不能被识别为对侧，避免放大阈值后与自身缝合。
TEST_CASE("GeometryTopologyEditor excludes same face boundaries from gap partners")
{
    const auto face = TopoDS::Face(GeometryBuilder::makeRectangleFace(0, 0, 0, 0.05, 10, CoordinatePlane::XY));
    const auto root = makeGeometryRoot(face);
    const auto seed = findEdgeOnX(face, 0.05);
    REQUIRE(GeometryTopologyEditor::findGapPartnerChains(root, { seed }, 1.0).empty());
    REQUIRE_THROWS_AS(GeometryTopologyEditor::expandStitchableFreeChain(root, seed, 1.0), std::runtime_error);
    REQUIRE_THROWS_AS(GeometryTopologyEditor::stitchGapFromSeedEdge(root, seed, 1.0), std::runtime_error);
}

//! @brief 最近匹配距离不固定为 0.01，较大真实间隙仍能在用户给定上限内缝合。
TEST_CASE("GeometryTopologyEditor searches actual larger gaps without a fixed distance cap")
{
    const auto left = TopoDS::Face(GeometryBuilder::makeRectangleFace(0, 0, 0, 10, 10, CoordinatePlane::XY));
    const auto right = TopoDS::Face(GeometryBuilder::makeRectangleFace(10.05, 0, 0, 10, 10, CoordinatePlane::XY));
    const auto root = makeShapePairRoot(left, right);
    const auto seed = findEdgeOnX(left, 10);
    REQUIRE_THROWS_AS(GeometryTopologyEditor::stitchGapFromSeedEdge(root, seed, 0.01), std::runtime_error);
    for (double tolerance : { 0.1, 1.0 }) {
        const auto result = GeometryTopologyEditor::stitchGapFromSeedEdge(root, seed, tolerance);
        REQUIRE(BRepCheck_Analyzer(result).IsValid());
        REQUIRE(countSharedEdges(result) == 1);
    }
}

//! @brief 正向整链移动因中间顶点支路失败时，反向应共享原边而保留支路及输入。
TEST_CASE("GeometryTopologyEditor retries the same stitch pair in reverse")
{
    const auto left = TopoDS::Face(GeometryBuilder::makeRectangleFace(0, 0, 0, 10, 10, CoordinatePlane::XY));
    const auto [right, edges] = makeSplitLeftRectangleFace(10.005, 0, 10, 10);
    TopoDS_Vertex middle;
    for (TopExp_Explorer v(right, TopAbs_VERTEX); v.More(); v.Next()) {
        const auto vertex = TopoDS::Vertex(v.Current());
        const auto point = BRep_Tool::Pnt(vertex);
        if (std::abs(point.X() - 10.005) < 1.e-7 && std::abs(point.Y() - 5) < 1.e-7)
            middle = vertex;
    }
    REQUIRE_FALSE(middle.IsNull());
    const auto tip = TopoDS::Vertex(GeometryBuilder::makePoint(10.005, 5, 2));
    const auto branch = BRepBuilderAPI_MakeEdge(middle, tip).Edge();
    BRep_Builder builder;
    auto root = makeShapePairRoot(left, right);
    builder.Add(root, branch);
    REQUIRE_THROWS_AS(GeometryTopologyEditor::stitchBoundaryEdges(root, edges, { findEdgeOnX(left, 10) }, 0.01), std::runtime_error);
    bool reversed = false;
    const auto result = GeometryTopologyEditor::stitchGapFromSeedEdge(root, edges.front(), 0.01, &reversed);
    REQUIRE(reversed);
    REQUIRE(BRepCheck_Analyzer(result).IsValid());
    REQUIRE(countSharedEdges(result) == 2);
    REQUIRE(countSubshapes(result, TopAbs_EDGE) == 9);
    REQUIRE(countSharedEdges(root) == 0);
    REQUIRE(BRep_Tool::Pnt(middle).X() == Catch::Approx(10.005));
}

//! @brief 两侧都无法整链移动时，合并报告两次失败原因且不修改输入。
TEST_CASE("GeometryTopologyEditor reports both failed stitch directions without changing the input")
{
    BRep_Builder builder;
    TopoDS_Compound root;
    builder.MakeCompound(root);
    TopoDS_Edge seed;
    for (double x : { 0.0, 0.005 }) {
        const auto [face, edges] = makeSplitLeftRectangleFace(x, 0, 10, 10);
        builder.Add(root, face);
        TopoDS_Vertex middle;
        for (TopExp_Explorer v(face, TopAbs_VERTEX); v.More(); v.Next()) {
            const auto vertex = TopoDS::Vertex(v.Current());
            if (BRep_Tool::Pnt(vertex).Distance(gp_Pnt(x, 5, 0)) < 1.e-7)
                middle = vertex;
        }
        const auto tip = TopoDS::Vertex(GeometryBuilder::makePoint(x, 5, 2));
        builder.Add(root, BRepBuilderAPI_MakeEdge(middle, tip).Edge());
        seed = edges.front();
    }
    const int before_edges = countSubshapes(root, TopAbs_EDGE);
    bool reversed = true;
    try {
        GeometryTopologyEditor::stitchGapFromSeedEdge(root, seed, 0.01, &reversed);
        FAIL("Both directions should fail");
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        REQUIRE(message.find("Selected side:") != std::string::npos);
        REQUIRE(message.find("Opposite side:") != std::string::npos);
    }
    REQUIRE_FALSE(reversed);
    REQUIRE(countSubshapes(root, TopAbs_EDGE) == before_edges);
    REQUIRE(countSharedEdges(root) == 0);
    REQUIRE(BRepCheck_Analyzer(root).IsValid());
}

//! @brief 正向可成功时不反向，输出标志不残留上一次状态。
TEST_CASE("GeometryTopologyEditor keeps the selected direction when it succeeds")
{
    const auto left = TopoDS::Face(GeometryBuilder::makeRectangleFace(0, 0, 0, 10, 10, CoordinatePlane::XY));
    const auto right = TopoDS::Face(GeometryBuilder::makeRectangleFace(10.005, 0, 0, 10, 10, CoordinatePlane::XY));
    bool reversed = true;
    const auto result = GeometryTopologyEditor::stitchGapFromSeedEdge(makeShapePairRoot(left, right), findEdgeOnX(left, 10), 0.01, &reversed);
    REQUIRE_FALSE(reversed);
    REQUIRE(countSharedEdges(result) == 1);
}

//! @brief 半圆柱侧面缺失时，应恢复贴合两条圆弧与母线的曲面，并连接全部原边。
TEST_CASE("GeometryTopologyEditor fills a missing half cylinder side")
{
    for (double radius : { 0.1, 1.0, 10.0 }) {
        CAPTURE(radius);
        const double height = radius * 2;
        const auto cylinder = GeometryBuilder::makeCylinder(0, 0, 0, radius, height, 0, 0, 1, std::acos(-1.0));
        TopoDS_Face side;
        for (TopExp_Explorer f(cylinder, TopAbs_FACE); f.More(); f.Next()) {
            if (BRepAdaptor_Surface(TopoDS::Face(f.Current())).GetType() == GeomAbs_Cylinder)
                side = TopoDS::Face(f.Current());
        }
        REQUIRE_FALSE(side.IsNull());
        const auto open = GeometryTopologyEditor::removeShape(makeGeometryRoot(cylinder), side, true);
        const auto seed = TopoDS::Edge(TopExp_Explorer(side, TopAbs_EDGE).Current());
        const auto result = GeometryTopologyEditor::fillBoundaryLoop(open, seed);
        REQUIRE(BRepCheck_Analyzer(result).IsValid());
        REQUIRE(countSubshapes(result, TopAbs_FACE) == countSubshapes(cylinder, TopAbs_FACE));
        REQUIRE(countSharedEdges(result) == countSharedEdges(cylinder));
        GProp_GProps before, after;
        BRepGProp::SurfaceProperties(cylinder, before);
        BRepGProp::SurfaceProperties(result, after);
        REQUIRE(after.Mass() == Catch::Approx(before.Mass()).epsilon(1.e-5));
    }
}

//! @brief 闭合边生成曲面后，局部缝合应连接新面的独立边，不必重新拟合已有曲面。
TEST_CASE("GeometryTopologyEditor stitches a separately created half cylinder patch")
{
    for (double radius : { 0.1, 1.0, 10.0 }) {
        CAPTURE(radius);
        const auto cylinder = GeometryBuilder::makeCylinder(0, 0, 0, radius, radius * 2, 0, 0, 1, std::acos(-1.0));
        TopoDS_Face side;
        for (TopExp_Explorer f(cylinder, TopAbs_FACE); f.More(); f.Next()) {
            if (BRepAdaptor_Surface(TopoDS::Face(f.Current())).GetType() == GeomAbs_Cylinder)
                side = TopoDS::Face(f.Current());
        }
        std::vector<TopoDS_Edge> boundary;
        for (TopExp_Explorer edge(side, TopAbs_EDGE); edge.More(); edge.Next())
            boundary.push_back(TopoDS::Edge(edge.Current()));
        const auto open = GeometryTopologyEditor::removeShape(makeGeometryRoot(cylinder), side, true);
        const auto patch = GeometryBuilder::makeFaceFromEdges(boundary);
        const auto root = makeShapePairRoot(open, patch);
        for (TopExp_Explorer edge(patch, TopAbs_EDGE); edge.More(); edge.Next()) {
            const auto seed = TopoDS::Edge(edge.Current());
            const auto result = GeometryTopologyEditor::stitchGapFromSeedEdge(root, seed, 0.01);
            REQUIRE(BRepCheck_Analyzer(result).IsValid());
            REQUIRE(countSubshapes(result, TopAbs_FACE) == countSubshapes(cylinder, TopAbs_FACE));
            REQUIRE(countSharedEdges(result) == countSharedEdges(cylinder));
            REQUIRE(countSubshapes(root, TopAbs_EDGE) > countSubshapes(result, TopAbs_EDGE));
        }
    }
}

//! @brief 从原孔洞的边选择局部缝合时，可连接单面边链，不要求源链跨越全部邻面。
TEST_CASE("GeometryTopologyEditor stitches from the original half cylinder rim")
{
    const auto cylinder = GeometryBuilder::makeCylinder(0, 0, 0, 1, 2, 0, 0, 1, std::acos(-1.0));
    TopoDS_Face side;
    for (TopExp_Explorer f(cylinder, TopAbs_FACE); f.More(); f.Next()) {
        if (BRepAdaptor_Surface(TopoDS::Face(f.Current())).GetType() == GeomAbs_Cylinder)
            side = TopoDS::Face(f.Current());
    }
    std::vector<TopoDS_Edge> boundary;
    for (TopExp_Explorer edge(side, TopAbs_EDGE); edge.More(); edge.Next())
        boundary.push_back(TopoDS::Edge(edge.Current()));
    const auto open = GeometryTopologyEditor::removeShape(makeGeometryRoot(cylinder), side, true);
    const auto patch = GeometryBuilder::makeFaceFromEdges(boundary);
    const auto root = makeShapePairRoot(open, patch);
    for (const auto& seed : boundary) {
        const auto result = GeometryTopologyEditor::stitchGapFromSeedEdge(root, seed, 0.01);
        REQUIRE(BRepCheck_Analyzer(result).IsValid());
        REQUIRE(countSharedEdges(result) > countSharedEdges(root));
        REQUIRE(countSubshapes(result, TopAbs_FACE) == countSubshapes(root, TopAbs_FACE));
    }
}

//! @brief 两条非圆曲线与两条直线围成的弯曲四边环，应直接生成连接原边的曲面。
TEST_CASE("GeometryTopologyEditor fills a general bent four edge boundary")
{
    for (double scale : { 0.1, 1.0, 10.0 }) {
        CAPTURE(scale);
        std::vector<TopoDS_Vertex> vertices;
        for (const auto& point : { gp_Pnt(0, 0, 0), gp_Pnt(10, 0, 0), gp_Pnt(10, 5, 0), gp_Pnt(0, 5, 0) })
            vertices.push_back(TopoDS::Vertex(GeometryBuilder::makePoint(point.X() * scale, point.Y() * scale, point.Z() * scale)));
        const auto curved_edge = [&](int start, int end, double y, double z1, double z2) {
            TColgp_Array1OfPnt poles(1, 4);
            poles.SetValue(1, BRep_Tool::Pnt(vertices[start]));
            poles.SetValue(2, gp_Pnt(1 * scale, y * scale, z1 * scale));
            poles.SetValue(3, gp_Pnt(9 * scale, y * scale, z2 * scale));
            poles.SetValue(4, BRep_Tool::Pnt(vertices[end]));
            occ::handle<Geom_BezierCurve> curve = new Geom_BezierCurve(poles);
            return BRepBuilderAPI_MakeEdge(curve, vertices[start], vertices[end]).Edge();
        };
        const std::vector<TopoDS_Edge> boundary {
            curved_edge(0, 1, 0, 8, -2),
            BRepBuilderAPI_MakeEdge(vertices[1], vertices[2]).Edge(),
            TopoDS::Edge(curved_edge(3, 2, 5, 5, 3).Reversed()),
            BRepBuilderAPI_MakeEdge(vertices[3], vertices[0]).Edge()
        };
        BRep_Builder builder;
        TopoDS_Compound root;
        builder.MakeCompound(root);
        for (const auto& edge : boundary)
            builder.Add(root, edge);
        const auto result = GeometryTopologyEditor::fillBoundaryLoop(root, boundary.front());
        REQUIRE(BRepCheck_Analyzer(result).IsValid());
        REQUIRE(countSubshapes(result, TopAbs_FACE) == 1);
        REQUIRE(countSubshapes(result, TopAbs_EDGE) == 4);
        REQUIRE(countSubshapes(result, TopAbs_VERTEX) == 4);
        REQUIRE(countSubshapes(root, TopAbs_FACE) == 0);
        // 在两条曲边外侧各加一个共面邻面，确认补面沿用原边并连接邻面。
        for (size_t index : { size_t(0), size_t(2) }) {
            TopoDS_Vertex first, last;
            TopExp::Vertices(boundary[index], first, last, true);
            const auto a = BRep_Tool::Pnt(first);
            const auto b = BRep_Tool::Pnt(last);
            const auto low_first = TopoDS::Vertex(GeometryBuilder::makePoint(a.X(), a.Y(), -10 * scale));
            const auto low_last = TopoDS::Vertex(GeometryBuilder::makePoint(b.X(), b.Y(), -10 * scale));
            BRepBuilderAPI_MakeWire support;
            support.Add(boundary[index]);
            support.Add(BRepBuilderAPI_MakeEdge(last, low_last).Edge());
            support.Add(BRepBuilderAPI_MakeEdge(low_last, low_first).Edge());
            support.Add(BRepBuilderAPI_MakeEdge(low_first, first).Edge());
            builder.Add(root, BRepBuilderAPI_MakeFace(support.Wire()).Face());
        }
        const auto joined = GeometryTopologyEditor::fillBoundaryLoop(root, TopoDS::Edge(boundary[2].Reversed()));
        REQUIRE(BRepCheck_Analyzer(joined).IsValid());
        REQUIRE(countSubshapes(joined, TopAbs_FACE) == 3);
        REQUIRE(countSharedEdges(joined) == 2);
        REQUIRE(countSubshapes(joined, TopAbs_EDGE) == 10);
    }
}

namespace {
/**
 * @brief 序列化输入形状，验证几何操作没有原地改写曲线、pcurve 或容差。
 */
std::string serializeRepairShape(const TopoDS_Shape& shape)
{
    std::ostringstream stream;
    BRepTools::Write(shape, stream);
    return stream.str();
}
}

//! @brief 粗容差既不能污染共面边，也不能把轻微翘曲的四边环误压成平面。
TEST_CASE("GeometryTopologyEditor isolates coarse tolerances when choosing a patch surface")
{
    for (double scale : { 0.1, 1.0, 100.0 }) {
        for (bool bent : { false, true }) {
            CAPTURE(scale, bent);
            const std::array<gp_Pnt, 4> points { gp_Pnt(0, 0, 0), gp_Pnt(10 * scale, 0, 0),
                gp_Pnt(10 * scale, 10 * scale, bent ? 0.001 * scale : 0), gp_Pnt(0, 10 * scale, 0) };
            BRepBuilderAPI_MakeWire wire;
            for (size_t i = 0; i < points.size(); ++i)
                wire.Add(BRepBuilderAPI_MakeEdge(points[i], points[(i + 1) % points.size()]).Edge());
            const auto root = wire.Wire();
            const auto seed = TopoDS::Edge(TopExp_Explorer(root, TopAbs_EDGE).Current());
            BRep_Builder builder;
            builder.UpdateEdge(seed, 0.01 * scale);
            // 保持输入的容差层级一致：边端点的容差不能小于该边。
            for (TopExp_Explorer vertex(seed, TopAbs_VERTEX); vertex.More(); vertex.Next())
                builder.UpdateVertex(TopoDS::Vertex(vertex.Current()), 0.01 * scale);
            const auto before = serializeRepairShape(root);
            const auto result = GeometryTopologyEditor::fillBoundaryLoop(root, seed);
            REQUIRE(BRepCheck_Analyzer(result).IsValid());
            REQUIRE(countSubshapes(result, TopAbs_FACE) == 1);
            const auto face = TopoDS::Face(TopExp_Explorer(result, TopAbs_FACE).Current());
            REQUIRE((BRepAdaptor_Surface(face).GetType() == GeomAbs_Plane) == !bent);
            int fine_edges = 0;
            for (TopExp_Explorer edge(face, TopAbs_EDGE); edge.More(); edge.Next()) {
                if (BRep_Tool::Tolerance(TopoDS::Edge(edge.Current())) < 0.001 * scale)
                    ++fine_edges;
            }
            REQUIRE(fine_edges == 3);
            REQUIRE(serializeRepairShape(root) == before);
        }
    }
}
