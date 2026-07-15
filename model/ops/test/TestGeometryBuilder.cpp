#include "GeometryBuilder.h"

#include <BRepCheck_Analyzer.hxx>
#include <TopAbs_ShapeEnum.hxx>

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>

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
