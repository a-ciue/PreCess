#include "GeometryRepairValidation.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepBndLib.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepLib_CheckCurveOnSurface.hxx>
#include <BRep_Tool.hxx>
#include <Bnd_Box.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <NCollection_Array1.hxx>
#include <NCollection_IndexedMap.hxx>
#include <Precision.hxx>
#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Pnt.hxx>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace geometry::repair {
PrecisionPolicy PrecisionPolicy::fromShape(const TopoDS_Shape& local_shape)
{
    if (local_shape.IsNull())
        throw std::invalid_argument("Cannot derive precision from an empty local shape");
    // 包围盒不使用已有实体容差膨胀，避免输入的粗容差反过来放宽新建几何。
    Bnd_Box box;
    BRepBndLib::AddOptimal(local_shape, box, false, false);
    if (box.IsVoid() || box.IsOpen())
        throw std::runtime_error("Cannot derive precision from unbounded geometry");
    double x0, y0, z0, x1, y1, z1;
    box.Get(x0, y0, z0, x1, y1, z1);
    const double length = gp_Pnt(x0, y0, z0).Distance(gp_Pnt(x1, y1, z1));
    const double coordinate = std::max({ std::abs(x0), std::abs(y0), std::abs(z0), std::abs(x1), std::abs(y1), std::abs(z1) });
    double shortest = std::numeric_limits<double>::infinity();
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edges;
    TopExp::MapShapes(local_shape, TopAbs_EDGE, edges);
    for (const auto& shape : edges) {
        const auto edge = TopoDS::Edge(shape);
        if (BRep_Tool::Degenerated(edge))
            continue;
        BRepAdaptor_Curve curve(edge);
        const double edge_length = GCPnts_AbscissaPoint::Length(curve);
        if (edge_length > 0.0 && std::isfinite(edge_length))
            shortest = std::min(shortest, edge_length);
    }
    PrecisionPolicy result;
    result.numerical_floor = std::max(Precision::Confusion(),
        64.0 * std::numeric_limits<double>::epsilon() * coordinate);
    // 相对精度集中定义：目标为局部尺度的百万分之一，且不超过最短边的千分之一。
    // 当特征已低于内核分辨能力时明确拒绝，不能靠增大容差吞掉它。
    const double feature_limit = shortest * 1.0e-3;
    if (!std::isfinite(length) || length <= 0.0 || feature_limit < result.numerical_floor)
        throw std::runtime_error("Local feature protection budget is below the kernel numerical floor");
    result.fitting = std::max(result.numerical_floor, std::min(length * 1.0e-6, feature_limit));
    result.validation = std::max(result.numerical_floor, std::min(result.fitting * 5.0, feature_limit));
    result.classification = std::max(result.numerical_floor, result.fitting * 0.1);
    return result;
}

DeviationResult measureCurveDeviation(const TopoDS_Edge& edge,
    const std::function<double(const gp_Pnt&)>& distance, double limit, const DeviationBudget& budget)
{
    if (!std::isfinite(limit) || limit <= 0.0 || budget.max_depth < 0 || budget.max_evaluations < 1
        || !std::isfinite(budget.resolution) || budget.resolution < 0.0)
        throw std::invalid_argument("Invalid deviation limit or computation budget");
    DeviationResult result;
    try {
        BRepAdaptor_Curve curve(edge);
        // 几何离散精度与查询距离分开，避免搜索阈值趋近零时把同一曲线耗尽预算。
        // 这里仅做候选预检，最终曲线/曲面一致性仍由 BoundaryAudit 的最大偏差算法验收。
        const double geometric_resolution = budget.resolution > 0.0 ? budget.resolution
            : std::max(limit * 0.25, PrecisionPolicy::fromShape(edge).fitting);
        if (!std::isfinite(curve.FirstParameter()) || !std::isfinite(curve.LastParameter())
            || curve.FirstParameter() >= curve.LastParameter())
            return result;
        // 每个 B-Spline 节点区间独立检查，避免窄峰恰好位于全局均匀采样点之间。
        const int count = curve.NbIntervals(GeomAbs_CN);
        if (count <= 0)
            return result;
        NCollection_Array1<double> intervals(1, count + 1);
        curve.Intervals(intervals, GeomAbs_CN);
        // 单个参数点的几何位置与偏差，递归时复用端点避免重复投影。
        struct Sample {
            gp_Pnt point;
            double deviation;
        };
        const auto evaluate = [&](double parameter, Sample& sample) {
            if (result.evaluations >= budget.max_evaluations)
                return false;
            ++result.evaluations;
            sample.point = curve.Value(parameter);
            sample.deviation = distance(sample.point);
            if (!std::isfinite(sample.deviation) || sample.deviation < 0.0)
                return false;
            result.maximum = std::max(result.maximum, sample.deviation);
            if (sample.deviation > limit) {
                result.status = DeviationStatus::Exceeded;
                return false;
            }
            return true;
        };
        std::function<bool(double, double, const Sample&, const Sample&, int)> subdivide;
        subdivide = [&](double first, double last, const Sample& a, const Sample& b, int depth) {
            const double middle = first + (last - first) * 0.5;
            if (middle <= first || middle >= last)
                return false;
            Sample m;
            if (!evaluate(middle, m))
                return false;
            const gp_Pnt chord_mid((a.point.X() + b.point.X()) * 0.5, (a.point.Y() + b.point.Y()) * 0.5, (a.point.Z() + b.point.Z()) * 0.5);
            const double bend = m.point.Distance(chord_mid);
            const double variation = std::abs(m.deviation - (a.deviation + b.deviation) * 0.5);
            // 至少细分三层；几何或残差尚未平稳时继续细分，不能把深度耗尽当成通过。
            const double resolution = budget.resolution > 0.0 ? budget.resolution : limit * 0.25;
            if (depth >= 3 && bend <= geometric_resolution && variation <= resolution)
                return true;
            if (depth >= budget.max_depth)
                return false;
            return subdivide(first, middle, a, m, depth + 1)
                && subdivide(middle, last, m, b, depth + 1);
        };
        for (int i = 1; i <= count; ++i) {
            Sample first, last;
            if (!evaluate(intervals(i), first) || !evaluate(intervals(i + 1), last)
                || !subdivide(intervals(i), intervals(i + 1), first, last, 0))
                return result;
        }
        result.status = DeviationStatus::WithinLimit;
    } catch (const Standard_Failure&) {
        result.status = DeviationStatus::Unresolved;
    }
    return result;
}

BoundaryAudit::BoundaryAudit(const TopoDS_Shape& boundary, const PrecisionPolicy& precision)
{
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edges, vertices;
    TopExp::MapShapes(boundary, TopAbs_EDGE, edges);
    TopExp::MapShapes(boundary, TopAbs_VERTEX, vertices);
    for (const auto& shape : edges) {
        const auto edge = TopoDS::Edge(shape);
        edges_.emplace_back(edge, std::max(precision.validation, BRep_Tool::Tolerance(edge)));
    }
    for (const auto& shape : vertices) {
        const auto vertex = TopoDS::Vertex(shape);
        vertices_.emplace_back(vertex, std::max(precision.validation, BRep_Tool::Tolerance(vertex)));
    }
}

void BoundaryAudit::validateFace(const TopoDS_Face& face, const char* operation) const
{
    const auto fail = [&](const std::string& detail) { throw std::runtime_error(std::string(operation) + ": " + detail); };
    if (face.IsNull() || !BRepCheck_Analyzer(face).IsValid())
        fail("invalid face topology");
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edges, vertices;
    TopExp::MapShapes(face, TopAbs_EDGE, edges);
    TopExp::MapShapes(face, TopAbs_VERTEX, vertices);
    if (edges.Extent() != static_cast<int>(edges_.size()) || vertices.Extent() != static_cast<int>(vertices_.size()))
        fail("boundary identity changed");
    for (const auto& [vertex, limit] : vertices_) {
        if (!vertices.Contains(vertex) || BRep_Tool::Tolerance(vertex) > limit)
            fail("vertex identity or tolerance budget exceeded");
    }
    for (const auto& [edge, limit] : edges_) {
        if (!edges.Contains(edge))
            fail("edge identity changed");
        if (BRep_Tool::Tolerance(edge) > limit) {
            std::ostringstream message;
            message << "edge tolerance=" << BRep_Tool::Tolerance(edge) << ", limit=" << limit;
            fail(message.str());
        }
        if (BRep_Tool::Degenerated(edge))
            continue;
        if (!BRep_Tool::SameParameter(edge) || !BRep_Tool::SameRange(edge))
            fail("edge has inconsistent curve parameters");
        BRepLib_CheckCurveOnSurface checker(edge, face);
        checker.Perform();
        if (!checker.IsDone())
            fail("cannot verify maximum curve-on-surface deviation");
        const double deviation = checker.MaxDistance();
        if (!std::isfinite(deviation) || deviation > limit) {
            std::ostringstream message;
            message << "curve-on-surface deviation=" << deviation << ", limit=" << limit;
            fail(message.str());
        }
    }
}
}
