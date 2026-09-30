/** @file TestGeometryRepairValidation.cpp
 * @brief 验证局部精度、节点自适应检测、计算预算和输入误差隔离。
 */
#include "GeometryRepairValidation.h"
#include "GeometryBuilder.h"
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRep_Tool.hxx>
#include <BRep_Builder.hxx>
#include <Geom_BSplineCurve.hxx>
#include <NCollection_Array1.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <gp_Trsf.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <cmath>
#include <stdexcept>

using namespace geometry::repair;

//! @brief 局部精度随模型尺度变化，平移与无关的输入粗容差不能改变求解目标。
TEST_CASE("GeometryRepair precision scales locally without inheriting coarse entity tolerances")
{
    const auto base = GeometryBuilder::makeRectangleFace(0, 0, 0, 10, 20, CoordinatePlane::XY);
    const auto reference = PrecisionPolicy::fromShape(base);
    for (double scale : { 0.01, 1.0, 100.0 }) {
        CAPTURE(scale);
        gp_Trsf transform;
        transform.SetScale(gp_Pnt(0, 0, 0), scale);
        const auto shape = BRepBuilderAPI_Transform(base, transform, true).Shape();
        const auto precision = PrecisionPolicy::fromShape(shape);
        REQUIRE(precision.fitting == Catch::Approx(reference.fitting * scale));
        REQUIRE(precision.validation == Catch::Approx(reference.validation * scale));
    }
    gp_Trsf translation;
    translation.SetTranslation(gp_Vec(1.e6, -1.e6, 1.e6));
    const auto translated = BRepBuilderAPI_Transform(base, translation, true).Shape();
    REQUIRE(PrecisionPolicy::fromShape(translated).fitting == Catch::Approx(reference.fitting));
    BRep_Builder builder;
    builder.UpdateEdge(TopoDS::Edge(TopExp_Explorer(base, TopAbs_EDGE).Current()), 0.5);
    REQUIRE(PrecisionPolicy::fromShape(base).fitting == Catch::Approx(reference.fitting));
}

//! @brief 节点之间的窄尖峰会避开旧的均匀九点采样，节点分段检查必须将其拒绝。
TEST_CASE("GeometryRepair adaptive deviation detects a narrow spline excursion")
{
    NCollection_Array1<gp_Pnt> poles(1, 5);
    NCollection_Array1<double> knots(1, 5);
    NCollection_Array1<int> multiplicities(1, 5);
    const double coordinates[] = { 0.0, 0.5001, 0.5002, 0.5003, 1.0 };
    for (int i = 1; i <= 5; ++i) {
        poles(i) = gp_Pnt(coordinates[i - 1], 0, i == 3 ? 0.01 : 0.0);
        knots(i) = coordinates[i - 1];
        multiplicities(i) = (i == 1 || i == 5) ? 2 : 1;
    }
    occ::handle<Geom_BSplineCurve> curve = new Geom_BSplineCurve(poles, knots, multiplicities, 1);
    const auto edge = BRepBuilderAPI_MakeEdge(curve).Edge();
    const auto distance = [](const gp_Pnt& p) { return std::abs(p.Z()); };
    REQUIRE(measureCurveDeviation(edge, distance, 1.e-4).status == DeviationStatus::Exceeded);
    REQUIRE(measureCurveDeviation(TopoDS::Edge(edge.Reversed()), distance, 1.e-4).status == DeviationStatus::Exceeded);
}

//! @brief 求值预算耗尽或投影无解时不能误报通过；调用方可以拒绝并保留原模型。
TEST_CASE("GeometryRepair reports unresolved deviation instead of accepting exhausted budgets")
{
    const auto edge = TopoDS::Edge(GeometryBuilder::makeLine(0, 0, 0, 10, 0, 0));
    REQUIRE(measureCurveDeviation(edge, [](const gp_Pnt&) { return 0.0; }, 1.e-5, { 16, 1 }).status == DeviationStatus::Unresolved);
    REQUIRE(measureCurveDeviation(edge, [](const gp_Pnt&) { return std::numeric_limits<double>::quiet_NaN(); }, 1.e-5).status == DeviationStatus::Unresolved);
    REQUIRE(measureCurveDeviation(edge, [](const gp_Pnt&) { return 0.0; }, 1.e-5).status == DeviationStatus::WithinLimit);
}

//! @brief 捕获修复前的实体级预算，禁止事后抬高容差绕过验收。
TEST_CASE("GeometryRepair audits boundary tolerance growth against the preoperation snapshot")
{
    const auto face = TopoDS::Face(GeometryBuilder::makeRectangleFace(0, 0, 0, 10, 20, CoordinatePlane::XY));
    const auto precision = PrecisionPolicy::fromShape(face);
    const BoundaryAudit audit(face, precision);
    REQUIRE_NOTHROW(audit.validateFace(face, "test"));
    BRep_Builder builder;
    builder.UpdateVertex(TopoDS::Vertex(TopExp_Explorer(face, TopAbs_VERTEX).Current()), precision.validation * 10);
    REQUIRE_THROWS_AS(audit.validateFace(face, "test"), std::runtime_error);
}

//! @brief 极小特征的保护预算与内核精度冲突时明确拒绝，不自动放宽或删除特征。
TEST_CASE("GeometryRepair refuses precision policies that would erase local features")
{
    const auto edge = GeometryBuilder::makeLine(0, 0, 0, 1.e-5, 0, 0);
    REQUIRE_THROWS_AS(PrecisionPolicy::fromShape(edge), std::runtime_error);
}
