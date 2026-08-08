#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "GeometryTopologyEditor.h"

#include <BRepCheck_Analyzer.hxx>
#include <BRep_Builder.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <TopTools_IndexedMapOfShape.hxx>

#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <utility>

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
    TopTools_IndexedMapOfShape shapes;
    TopExp::MapShapes(shape, type, shapes);
    return shapes.Extent();
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
