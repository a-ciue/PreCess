#include "GeometryBuilder.h"

#include <BRepCheck_Analyzer.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <limits>
#include <stdexcept>
#include <vector>

TEST_CASE("GeometryBuilder creates a valid point")
{
    TopoDS_Shape shape = GeometryBuilder::makePoint(1.0, 2.0, 3.0);

    REQUIRE_FALSE(shape.IsNull());
    REQUIRE(shape.ShapeType() == TopAbs_VERTEX);
    REQUIRE(BRepCheck_Analyzer(shape).IsValid());
}

TEST_CASE("GeometryBuilder rejects non-finite point coordinates")
{
    REQUIRE_THROWS_AS(
        GeometryBuilder::makePoint(
            std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0),
        std::invalid_argument);
}

TEST_CASE("GeometryBuilder creates a valid line from coordinates")
{
    TopoDS_Shape shape = GeometryBuilder::makeLine(
        0.0, 0.0, 0.0, 10.0, 0.0, 0.0);

    REQUIRE_FALSE(shape.IsNull());
    REQUIRE(shape.ShapeType() == TopAbs_EDGE);
    REQUIRE(BRepCheck_Analyzer(shape).IsValid());
}

TEST_CASE("GeometryBuilder line reuses selected vertices")
{
    const TopoDS_Vertex start = TopoDS::Vertex(
        GeometryBuilder::makePoint(0.0, 0.0, 0.0));
    const TopoDS_Vertex end = TopoDS::Vertex(
        GeometryBuilder::makePoint(10.0, 0.0, 0.0));

    const TopoDS_Edge edge = TopoDS::Edge(
        GeometryBuilder::makeLine(start, end));
    TopoDS_Vertex edge_start;
    TopoDS_Vertex edge_end;
    TopExp::Vertices(edge, edge_start, edge_end);

    REQUIRE((edge_start.IsSame(start) && edge_end.IsSame(end)
        || edge_start.IsSame(end) && edge_end.IsSame(start)));
}

TEST_CASE("GeometryBuilder rejects coincident line endpoints")
{
    REQUIRE_THROWS_AS(
        GeometryBuilder::makeLine(0.0, 0.0, 0.0, 0.0, 0.0, 0.0),
        std::invalid_argument);
}

TEST_CASE("GeometryBuilder creates valid rectangular faces in supported planes")
{
    const std::array<CoordinatePlane, 3> planes {
        CoordinatePlane::XY,
        CoordinatePlane::YZ,
        CoordinatePlane::XZ
    };

    for (CoordinatePlane plane : planes) {
        TopoDS_Shape shape = GeometryBuilder::makeRectangleFace(
            1.0, 2.0, 3.0, 10.0, 20.0, plane);

        REQUIRE_FALSE(shape.IsNull());
        REQUIRE(shape.ShapeType() == TopAbs_FACE);
        REQUIRE(BRepCheck_Analyzer(shape).IsValid());
    }
}

TEST_CASE("GeometryBuilder rejects non-positive rectangle face dimensions")
{
    REQUIRE_THROWS_AS(
        GeometryBuilder::makeRectangleFace(
            0.0, 0.0, 0.0, 0.0, 1.0, CoordinatePlane::XY),
        std::invalid_argument);
}

TEST_CASE("GeometryBuilder creates valid disk faces in supported planes")
{
    const std::array<CoordinatePlane, 3> planes {
        CoordinatePlane::XY,
        CoordinatePlane::YZ,
        CoordinatePlane::XZ
    };

    for (CoordinatePlane plane : planes) {
        TopoDS_Shape shape = GeometryBuilder::makeDiskFace(
            1.0, 2.0, 3.0, 10.0, plane);

        REQUIRE_FALSE(shape.IsNull());
        REQUIRE(shape.ShapeType() == TopAbs_FACE);
        REQUIRE(BRepCheck_Analyzer(shape).IsValid());
    }
}

TEST_CASE("GeometryBuilder rejects non-positive disk face radius")
{
    REQUIRE_THROWS_AS(
        GeometryBuilder::makeDiskFace(
            0.0, 0.0, 0.0, 0.0, CoordinatePlane::XY),
        std::invalid_argument);
}

TEST_CASE("GeometryBuilder creates a planar face from unordered closed edges")
{
    const TopoDS_Shape rectangle = GeometryBuilder::makeRectangleFace(
        0.0, 0.0, 0.0, 10.0, 20.0, CoordinatePlane::XY);
    std::vector<TopoDS_Edge> edges;
    for (TopExp_Explorer exp(rectangle, TopAbs_EDGE); exp.More(); exp.Next())
        edges.push_back(TopoDS::Edge(exp.Current()));
    REQUIRE(edges.size() == 4);
    const std::vector<TopoDS_Edge> source_edges = edges;
    edges = { source_edges[0], source_edges[2], source_edges[1], source_edges[3] };

    const TopoDS_Shape shape = GeometryBuilder::makeFaceFromEdges(edges);

    REQUIRE_FALSE(shape.IsNull());
    REQUIRE(shape.ShapeType() == TopAbs_FACE);
    REQUIRE(BRepCheck_Analyzer(shape).IsValid());
}

TEST_CASE("GeometryBuilder creates a filling face from non-planar closed edges")
{
    const std::array<TopoDS_Vertex, 4> vertices {
        TopoDS::Vertex(GeometryBuilder::makePoint(0.0, 0.0, 0.0)),
        TopoDS::Vertex(GeometryBuilder::makePoint(10.0, 0.0, 0.0)),
        TopoDS::Vertex(GeometryBuilder::makePoint(10.0, 10.0, 5.0)),
        TopoDS::Vertex(GeometryBuilder::makePoint(0.0, 10.0, 0.0))
    };
    std::vector<TopoDS_Edge> edges;
    for (size_t i = 0; i < vertices.size(); ++i) {
        edges.push_back(TopoDS::Edge(GeometryBuilder::makeLine(
            vertices[i], vertices[(i + 1) % vertices.size()])));
    }

    const TopoDS_Shape shape = GeometryBuilder::makeFaceFromEdges(edges);

    REQUIRE_FALSE(shape.IsNull());
    REQUIRE(shape.ShapeType() == TopAbs_FACE);
    REQUIRE(BRepCheck_Analyzer(shape).IsValid());
}

TEST_CASE("GeometryBuilder rejects an open edge chain")
{
    std::vector<TopoDS_Edge> edges {
        TopoDS::Edge(GeometryBuilder::makeLine(0.0, 0.0, 0.0, 10.0, 0.0, 0.0)),
        TopoDS::Edge(GeometryBuilder::makeLine(10.0, 0.0, 0.0, 10.0, 10.0, 0.0))
    };

    REQUIRE_THROWS_AS(
        GeometryBuilder::makeFaceFromEdges(edges),
        std::invalid_argument);
}

TEST_CASE("GeometryBuilder creates a valid box")
{
    TopoDS_Shape shape = GeometryBuilder::makeBox(1.0, 2.0, 3.0, 10.0, 20.0, 30.0);

    REQUIRE_FALSE(shape.IsNull());
    REQUIRE(shape.ShapeType() == TopAbs_SOLID);
    REQUIRE(BRepCheck_Analyzer(shape).IsValid());
}

TEST_CASE("GeometryBuilder rejects non-positive box dimensions")
{
    REQUIRE_THROWS_AS(
        GeometryBuilder::makeBox(0.0, 0.0, 0.0, 0.0, 1.0, 1.0),
        std::invalid_argument);
}

TEST_CASE("GeometryBuilder creates a valid cylinder")
{
    TopoDS_Shape shape = GeometryBuilder::makeCylinder(
        1.0, 2.0, 3.0,
        10.0, 20.0,
        0.0, 0.0, 1.0);

    REQUIRE_FALSE(shape.IsNull());
    REQUIRE(shape.ShapeType() == TopAbs_SOLID);
    REQUIRE(BRepCheck_Analyzer(shape).IsValid());
}

TEST_CASE("GeometryBuilder rejects invalid cylinder parameters")
{
    REQUIRE_THROWS_AS(
        GeometryBuilder::makeCylinder(
            0.0, 0.0, 0.0,
            0.0, 10.0,
            0.0, 0.0, 1.0),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        GeometryBuilder::makeCylinder(
            0.0, 0.0, 0.0,
            5.0, 10.0,
            0.0, 0.0, 0.0),
        std::invalid_argument);
}

TEST_CASE("GeometryBuilder extrudes a copied face into a valid solid")
{
    const TopoDS_Face source = TopoDS::Face(
        GeometryBuilder::makeRectangleFace(
            0.0, 0.0, 0.0, 10.0, 20.0, CoordinatePlane::XY));

    TopoDS_Shape shape = GeometryBuilder::extrudeFace(
        source, 0.0, 0.0, 1.0, 30.0);

    REQUIRE_FALSE(shape.IsNull());
    REQUIRE(TopExp_Explorer(shape, TopAbs_SOLID).More());
    REQUIRE(BRepCheck_Analyzer(shape).IsValid());

    bool shares_source_face = false;
    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) {
        if (exp.Current().IsSame(source)) {
            shares_source_face = true;
            break;
        }
    }
    REQUIRE_FALSE(shares_source_face);
}

TEST_CASE("GeometryBuilder rejects invalid face extrusion parameters")
{
    const TopoDS_Face source = TopoDS::Face(
        GeometryBuilder::makeRectangleFace(
            0.0, 0.0, 0.0, 10.0, 20.0, CoordinatePlane::XY));

    REQUIRE_THROWS_AS(
        GeometryBuilder::extrudeFace(source, 0.0, 0.0, 0.0, 10.0),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        GeometryBuilder::extrudeFace(source, 0.0, 0.0, 1.0, 0.0),
        std::invalid_argument);
}
