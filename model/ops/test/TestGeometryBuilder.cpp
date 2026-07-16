#include "GeometryBuilder.h"

#include <BRepCheck_Analyzer.hxx>
#include <TopExp.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <limits>
#include <stdexcept>

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
