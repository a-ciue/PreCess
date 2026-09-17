#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "GeometryTopologyEditor.h"

#include <BRepCheck_Analyzer.hxx>
#include <BRep_Tool.hxx>
#include <BRep_Builder.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Pnt.hxx>

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
    REQUIRE(result.intersecting_face_pairs.empty());
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
    REQUIRE(result.intersecting_face_pairs.size() == 1);
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
