#include "GeometryTopologyEditor.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepCheck_Status.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepExtrema_SelfIntersection.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <BRep_Builder.hxx>
#include <BRepTools.hxx>
#include <Bnd_Box.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom_BezierSurface.hxx>
#include <Geom_Surface.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GProp_GProps.hxx>
#include <IntCurveSurface_TransitionOnCurve.hxx>
#include <Precision.hxx>
#include <Poly_Triangulation.hxx>
#include <NCollection_Array1.hxx>
#include <NCollection_Array2.hxx>
#include <NCollection_DataMap.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopAbs_State.hxx>
#include <TopExp.hxx>
#include <TopLoc_Location.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_List.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS.hxx>
#include <TColStd_PackedMapOfInteger.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>
#include <TopExp_Explorer.hxx>
#include <algorithm>
#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <exception>
#include <execution>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
// 返回非级联删除时需要保留的直接下级拓扑类型。
TopAbs_ShapeEnum lowerShapeType(TopAbs_ShapeEnum type)
{
    switch (type) {
    case TopAbs_SOLID:
        return TopAbs_FACE;
    case TopAbs_FACE:
        return TopAbs_EDGE;
    case TopAbs_EDGE:
        return TopAbs_VERTEX;
    default:
        return TopAbs_SHAPE;
    }
}

bool isSupportedShapeType(TopAbs_ShapeEnum type)
{
    return type == TopAbs_VERTEX || type == TopAbs_EDGE
        || type == TopAbs_FACE || type == TopAbs_SOLID;
}


/**
 * @brief 返回列表中按 OCC 拓扑身份去重后的 Face 数量。
 */
int uniqueFaceCount(const NCollection_List<TopoDS_Shape>& ancestors)
{
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> faces;
    for (const TopoDS_Shape& ancestor : ancestors)
        faces.Add(ancestor);
    return faces.Extent();
}

/**
 * @brief 描述一块面域的有向包围盒，用作严格排除不相交面组合的保守边界。
 *
 * 边界盒必须完整盖住它代表的面域，因此"两个边界盒相距超过容差"才能证明
 * 对应的两块面域没有公共点；判定只允许在这个方向上给出结论。
 */
struct FaceBoundBox {
    std::array<double, 3> center {};
    std::array<std::array<double, 3>, 3> axes { { { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 } } };
    std::array<double, 3> half {};
};

double dotProduct(const std::array<double, 3>& first, const std::array<double, 3>& second)
{
    return first[0] * second[0] + first[1] * second[1] + first[2] * second[2];
}

std::array<double, 3> crossProduct(
    const std::array<double, 3>& first, const std::array<double, 3>& second)
{
    return { first[1] * second[2] - first[2] * second[1],
        first[2] * second[0] - first[0] * second[2],
        first[0] * second[1] - first[1] * second[0] };
}

double vectorLength(const std::array<double, 3>& vector)
{
    return std::sqrt(dotProduct(vector, vector));
}

std::array<double, 3> normalizedVector(const std::array<double, 3>& vector)
{
    const double length = vectorLength(vector);
    if (length <= 0.0)
        return { 0.0, 0.0, 0.0 };
    return { vector[0] / length, vector[1] / length, vector[2] / length };
}

std::array<double, 3> directionOf(const gp_Dir& direction)
{
    return { direction.X(), direction.Y(), direction.Z() };
}

std::array<double, 3> pointOf(const gp_Pnt& point)
{
    return { point.X(), point.Y(), point.Z() };
}

/**
 * @brief 构造与坐标轴对齐的保守边界盒，并按给定余量外扩。
 */
FaceBoundBox axisAlignedBound(
    const std::array<double, 3>& minimum,
    const std::array<double, 3>& maximum,
    double margin)
{
    FaceBoundBox bound;
    for (size_t axis = 0; axis < 3; ++axis) {
        bound.center[axis] = 0.5 * (minimum[axis] + maximum[axis]);
        bound.half[axis] = 0.5 * (maximum[axis] - minimum[axis]) + margin;
    }
    return bound;
}

/**
 * @brief 把控制点下标区间收拢到有效范围。
 */
std::pair<int, int> clampRange(int low, int high, int count)
{
    return { std::max(1, low), std::min(count, high) };
}

/**
 * @brief 合并相邻的区间，把区间数量压到上限以内，保持区间仍然连续且覆盖原范围。
 */
void limitRangeCount(std::vector<std::pair<int, int>>& ranges, int maximum_count)
{
    while (static_cast<int>(ranges.size()) > maximum_count) {
        std::vector<std::pair<int, int>> merged;
        merged.reserve(ranges.size() / 2 + 1);
        for (size_t index = 0; index < ranges.size(); index += 2) {
            const int low = ranges[index].first;
            const int high = index + 1 < ranges.size() ? ranges[index + 1].second
                                                      : ranges[index].second;
            merged.emplace_back(low, high);
        }
        ranges.swap(merged);
    }
}

/**
 * @brief 按控制点下标区间逐块生成自由曲面的保守边界盒。
 *
 * B-Spline 曲面在单个节点区间上位于其支撑控制点的凸包内，因此按区间取控制点
 * 的轴向包围盒仍然是该区间曲面的保守边界；合并区间只会让边界变松，不会漏掉曲面。
 */
void appendFreeFormBounds(
    const NCollection_Array2<gp_Pnt>& poles,
    const std::vector<std::pair<int, int>>& row_ranges,
    const std::vector<std::pair<int, int>>& column_ranges,
    double margin,
    std::vector<FaceBoundBox>& bounds)
{
    bounds.reserve(bounds.size() + row_ranges.size() * column_ranges.size());
    for (const auto& [row_low, row_high] : row_ranges) {
        for (const auto& [column_low, column_high] : column_ranges) {
            if (row_low > row_high || column_low > column_high)
                continue;
            std::array<double, 3> minimum = { std::numeric_limits<double>::max(),
                std::numeric_limits<double>::max(), std::numeric_limits<double>::max() };
            std::array<double, 3> maximum = { std::numeric_limits<double>::lowest(),
                std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest() };
            for (int row = row_low; row <= row_high; ++row) {
                for (int column = column_low; column <= column_high; ++column) {
                    const gp_Pnt& point = poles(row, column);
                    minimum[0] = std::min(minimum[0], point.X());
                    minimum[1] = std::min(minimum[1], point.Y());
                    minimum[2] = std::min(minimum[2], point.Z());
                    maximum[0] = std::max(maximum[0], point.X());
                    maximum[1] = std::max(maximum[1], point.Y());
                    maximum[2] = std::max(maximum[2], point.Z());
                }
            }
            bounds.push_back(axisAlignedBound(minimum, maximum, margin));
        }
    }
}

/**
 * @brief 生成面的保守边界盒集合，用于在执行精确求交前排除不相交的面组合。
 *
 * 平面使用支撑平面的零厚度板片和面内矩形（线性函数在包围盒角点上取极值，
 * 因此角点投影即可盖住整个面）；自由曲面使用节点区间内控制点的凸包；
 * 其余曲面退化为外扩后的最优包围盒。所有边界盒都按面的形状容差外扩，
 * 保证"边界盒相距超过容差"蕴含"面域相距超过容差"。
 */
std::vector<FaceBoundBox> faceBoundBoxes(
    const TopoDS_Face& face,
    const BRepAdaptor_Surface& surface,
    const std::array<double, 3>& minimum,
    const std::array<double, 3>& maximum)
{
    const double margin = BRep_Tool::Tolerance(face);
    std::vector<FaceBoundBox> bounds;
    const occ::handle<Geom_Surface> geometry = BRep_Tool::Surface(face);
    if (geometry.IsNull()) {
        bounds.push_back(axisAlignedBound(minimum, maximum, margin));
        return bounds;
    }

    if (surface.GetType() == GeomAbs_Plane) {
        const gp_Ax3 frame = surface.Plane().Position();
        const std::array<double, 3> normal = directionOf(frame.Direction());
        const std::array<double, 3> x_direction = directionOf(frame.XDirection());
        const std::array<double, 3> y_direction = directionOf(frame.YDirection());
        const std::array<double, 3> origin = pointOf(frame.Location());
        double u_minimum = 1e300;
        double u_maximum = -1e300;
        double v_minimum = 1e300;
        double v_maximum = -1e300;
        for (int corner_index = 0; corner_index < 8; ++corner_index) {
            const std::array<double, 3> corner { (corner_index & 1) ? maximum[0] : minimum[0],
                (corner_index & 2) ? maximum[1] : minimum[1],
                (corner_index & 4) ? maximum[2] : minimum[2] };
            const double u = dotProduct(corner, x_direction);
            const double v = dotProduct(corner, y_direction);
            u_minimum = std::min(u_minimum, u);
            u_maximum = std::max(u_maximum, u);
            v_minimum = std::min(v_minimum, v);
            v_maximum = std::max(v_maximum, v);
        }
        const double offset = dotProduct(origin, normal);
        const double u_center = 0.5 * (u_minimum + u_maximum);
        const double v_center = 0.5 * (v_minimum + v_maximum);
        FaceBoundBox slab;
        slab.axes[0] = normal;
        slab.axes[1] = x_direction;
        slab.axes[2] = y_direction;
        for (size_t axis = 0; axis < 3; ++axis) {
            slab.center[axis]
                = normal[axis] * offset + x_direction[axis] * u_center + y_direction[axis] * v_center;
        }
        slab.half[0] = margin;
        slab.half[1] = 0.5 * (u_maximum - u_minimum) + margin;
        slab.half[2] = 0.5 * (v_maximum - v_minimum) + margin;
        bounds.push_back(slab);
        return bounds;
    }

    if (surface.GetType() == GeomAbs_BSplineSurface) {
        const occ::handle<Geom_BSplineSurface> bspline
            = occ::handle<Geom_BSplineSurface>::DownCast(geometry);
        if (!bspline.IsNull()) {
            const NCollection_Array2<gp_Pnt>& poles = bspline->Poles();
            // 必须使用带重复节点的节点序列：基函数 N(i) 的支撑区间是
            // [U(i), U(i + degree + 1)]，下标基于展开后的序列，而不是去重节点表。
            const NCollection_Array1<double>& knots_u = bspline->UKnotSequence();
            const NCollection_Array1<double>& knots_v = bspline->VKnotSequence();
            double u_first = 0.0;
            double u_last = 0.0;
            double v_first = 0.0;
            double v_last = 0.0;
            BRepTools::UVBounds(face, u_first, u_last, v_first, v_last);
            const bool valid_bounds = std::isfinite(u_first) && std::isfinite(u_last)
                && std::isfinite(v_first) && std::isfinite(v_last) && u_first < u_last
                && v_first < v_last;

            // 只保留与面参数域相交的非退化节点区间。
            std::vector<std::pair<int, int>> spans_u;
            std::vector<std::pair<int, int>> spans_v;
            for (int span = knots_u.Lower(); span < knots_u.Upper(); ++span) {
                if (knots_u(span + 1) <= knots_u(span))
                    continue;
                if (valid_bounds && (knots_u(span + 1) <= u_first || knots_u(span) >= u_last))
                    continue;
                spans_u.emplace_back(span, span);
            }
            for (int span = knots_v.Lower(); span < knots_v.Upper(); ++span) {
                if (knots_v(span + 1) <= knots_v(span))
                    continue;
                if (valid_bounds && (knots_v(span + 1) <= v_first || knots_v(span) >= v_last))
                    continue;
                spans_v.emplace_back(span, span);
            }
            if (spans_u.empty())
                spans_u.emplace_back(knots_u.Lower(), knots_u.Upper() - 1);
            if (spans_v.empty())
                spans_v.emplace_back(knots_v.Lower(), knots_v.Upper() - 1);

            // 区间数量决定逐块判定的成本，合并到上限以内只损失紧致度。
            constexpr int maximum_cells_per_axis = 16;
            limitRangeCount(spans_u, maximum_cells_per_axis);
            limitRangeCount(spans_v, maximum_cells_per_axis);
            std::vector<std::pair<int, int>> row_ranges;
            std::vector<std::pair<int, int>> column_ranges;
            row_ranges.reserve(spans_u.size());
            column_ranges.reserve(spans_v.size());
            // 区间 [k1, k2] 的曲面位于 N(k1 - degree)..N(k2) 控制点的凸包内。
            for (const auto& [first_span, last_span] : spans_u) {
                row_ranges.push_back(clampRange(first_span - bspline->UDegree(), last_span,
                    bspline->NbUPoles()));
            }
            for (const auto& [first_span, last_span] : spans_v) {
                column_ranges.push_back(clampRange(first_span - bspline->VDegree(), last_span,
                    bspline->NbVPoles()));
            }
            appendFreeFormBounds(poles, row_ranges, column_ranges, margin, bounds);
            if (bounds.empty())
                bounds.push_back(axisAlignedBound(minimum, maximum, margin));
            return bounds;
        }
    }

    if (surface.GetType() == GeomAbs_BezierSurface) {
        const occ::handle<Geom_BezierSurface> bezier
            = occ::handle<Geom_BezierSurface>::DownCast(geometry);
        if (!bezier.IsNull()) {
            const NCollection_Array2<gp_Pnt>& poles = bezier->Poles();
            const std::vector<std::pair<int, int>> row_ranges {
                std::make_pair(poles.LowerRow(), poles.UpperRow())
            };
            const std::vector<std::pair<int, int>> column_ranges {
                std::make_pair(poles.LowerCol(), poles.UpperCol())
            };
            appendFreeFormBounds(poles, row_ranges, column_ranges, margin, bounds);
            if (bounds.empty())
                bounds.push_back(axisAlignedBound(minimum, maximum, margin));
            return bounds;
        }
    }

    bounds.push_back(axisAlignedBound(minimum, maximum, margin));
    return bounds;
}

/**
 * @brief 判断两个保守边界盒的间距是否大于给定容差。
 *
 * 只返回"确定分离"的结论：在任一候选方向上投影区间相距超过容差即成立，
 * 找不到分离方向时返回 false，交由精确判定处理。
 */
bool boundBoxesSeparated(const FaceBoundBox& first, const FaceBoundBox& second, double tolerance)
{
    std::array<std::array<double, 3>, 15> directions;
    size_t direction_count = 0;
    for (size_t axis = 0; axis < 3; ++axis)
        directions[direction_count++] = first.axes[axis];
    for (size_t axis = 0; axis < 3; ++axis)
        directions[direction_count++] = second.axes[axis];
    for (size_t first_axis = 0; first_axis < 3; ++first_axis) {
        for (size_t second_axis = 0; second_axis < 3; ++second_axis) {
            const std::array<double, 3> cross = normalizedVector(
                crossProduct(first.axes[first_axis], second.axes[second_axis]));
            if (vectorLength(cross) > 0.5)
                directions[direction_count++] = cross;
        }
    }

    for (size_t index = 0; index < direction_count; ++index) {
        const std::array<double, 3>& direction = directions[index];
        double first_low = dotProduct(first.center, direction);
        double first_high = first_low;
        double second_low = dotProduct(second.center, direction);
        double second_high = second_low;
        for (size_t axis = 0; axis < 3; ++axis) {
            const double first_radius = first.half[axis] * std::abs(dotProduct(first.axes[axis], direction));
            first_low -= first_radius;
            first_high += first_radius;
            const double second_radius
                = second.half[axis] * std::abs(dotProduct(second.axes[axis], direction));
            second_low -= second_radius;
            second_high += second_radius;
        }
        if (first_high + tolerance < second_low || second_high + tolerance < first_low)
            return true;
    }
    return false;
}

/**
 * @brief 保存重复面和相交面筛选需要的低成本几何量。
 */
struct FaceMetrics {
    TopoDS_Face face;
    Bnd_Box box;
    std::vector<FaceBoundBox> bounds;
    std::vector<TopoDS_Edge> edges;
    gp_Pnt center;
    gp_Pln plane;
    gp_Cylinder cylinder;
    std::array<double, 3> minimum {};
    std::array<double, 3> maximum {};
    double area { 0.0 };
    double perimeter { 0.0 };
    GeomAbs_SurfaceType surface_type { GeomAbs_OtherSurface };
};

FaceMetrics measureFace(const TopoDS_Face& face)
{
    FaceMetrics metrics;
    metrics.face = face;
    BRepBndLib::AddOptimal(face, metrics.box, false);
    metrics.box.Get(metrics.minimum[0], metrics.minimum[1], metrics.minimum[2],
        metrics.maximum[0], metrics.maximum[1], metrics.maximum[2]);
    for (TopExp_Explorer edge(face, TopAbs_EDGE); edge.More(); edge.Next())
        metrics.edges.push_back(TopoDS::Edge(edge.Current()));

    GProp_GProps surface_properties;
    BRepGProp::SurfaceProperties(face, surface_properties);
    metrics.area = std::abs(surface_properties.Mass());
    metrics.center = surface_properties.CentreOfMass();

    GProp_GProps linear_properties;
    BRepGProp::LinearProperties(face, linear_properties);
    metrics.perimeter = std::abs(linear_properties.Mass());
    BRepAdaptor_Surface surface(face);
    metrics.surface_type = surface.GetType();
    if (metrics.surface_type == GeomAbs_Plane)
        metrics.plane = surface.Plane();
    else if (metrics.surface_type == GeomAbs_Cylinder)
        metrics.cylinder = surface.Cylinder();
    metrics.bounds = faceBoundBoxes(face, surface, metrics.minimum, metrics.maximum);
    return metrics;
}

/**
 * @brief 用保守边界证明两面在容差内不可能有公共点。
 *
 * 每个边界盒都完整覆盖它代表的面域。只有**每一对**盒组合都能证明分离时，才说明两
 * 面必定没有公共点，返回 true；只要存在一对盒组合无法分离（两面可能在该区域附近
 * 相交或同域重叠），就返回 false，交由精确判定处理。
 *
 * 注意返回 false 只表示保守边界无法证明分离，不表示两面一定相交。
 */
bool facesSeparated(const FaceMetrics& first, const FaceMetrics& second, double tolerance)
{
    for (const FaceBoundBox& first_bound : first.bounds) {
        for (const FaceBoundBox& second_bound : second.bounds) {
            if (!boundBoxesSeparated(first_bound, second_bound, tolerance))
                return false;
        }
    }
    return true;
}

/**
 * @brief 用包围盒扫掠生成可能相交的面组合，避免对全部面执行二次遍历。
 */
std::vector<std::pair<size_t, size_t>> overlappingFacePairs(
    const std::vector<FaceMetrics>& faces,
    double tolerance)
{
    if (faces.empty())
        return {};

    std::array<double, 3> overall_minimum = faces.front().minimum;
    std::array<double, 3> overall_maximum = faces.front().maximum;
    for (const FaceMetrics& face : faces) {
        for (size_t axis = 0; axis < 3; ++axis) {
            overall_minimum[axis] = std::min(overall_minimum[axis], face.minimum[axis]);
            overall_maximum[axis] = std::max(overall_maximum[axis], face.maximum[axis]);
        }
    }
    size_t sweep_axis = 0;
    for (size_t axis = 1; axis < 3; ++axis) {
        if (overall_maximum[axis] - overall_minimum[axis]
            > overall_maximum[sweep_axis] - overall_minimum[sweep_axis]) {
            sweep_axis = axis;
        }
    }

    std::vector<size_t> order(faces.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&faces, sweep_axis](size_t first, size_t second) {
        return faces[first].minimum[sweep_axis] < faces[second].minimum[sweep_axis];
    });

    std::vector<std::pair<size_t, size_t>> pairs;
    for (size_t first_order = 0; first_order < order.size(); ++first_order) {
        const size_t first = order[first_order];
        for (size_t second_order = first_order + 1; second_order < order.size(); ++second_order) {
            const size_t second = order[second_order];
            if (faces[second].minimum[sweep_axis]
                > faces[first].maximum[sweep_axis] + tolerance) {
                break;
            }
            bool overlaps = true;
            for (size_t axis = 0; axis < 3; ++axis) {
                if (axis == sweep_axis)
                    continue;
                if (faces[first].maximum[axis] + tolerance < faces[second].minimum[axis]
                    || faces[second].maximum[axis] + tolerance < faces[first].minimum[axis]) {
                    overlaps = false;
                    break;
                }
            }
            if (!overlaps)
                continue;
            pairs.emplace_back(first, second);
        }
    }
    return pairs;
}

bool nearlyEqual(double first, double second, double absolute_tolerance)
{
    const double relative_tolerance = std::max(std::abs(first), std::abs(second)) * 1.0e-7;
    return std::abs(first - second) <= std::max(absolute_tolerance, relative_tolerance);
}

bool boxesMatch(const Bnd_Box& first, const Bnd_Box& second, double tolerance)
{
    if (first.IsVoid() || second.IsVoid())
        return false;
    double first_min_x = 0.0;
    double first_min_y = 0.0;
    double first_min_z = 0.0;
    double first_max_x = 0.0;
    double first_max_y = 0.0;
    double first_max_z = 0.0;
    double second_min_x = 0.0;
    double second_min_y = 0.0;
    double second_min_z = 0.0;
    double second_max_x = 0.0;
    double second_max_y = 0.0;
    double second_max_z = 0.0;
    first.Get(first_min_x, first_min_y, first_min_z,
        first_max_x, first_max_y, first_max_z);
    second.Get(second_min_x, second_min_y, second_min_z,
        second_max_x, second_max_y, second_max_z);
    return std::abs(first_min_x - second_min_x) <= tolerance
        && std::abs(first_min_y - second_min_y) <= tolerance
        && std::abs(first_min_z - second_min_z) <= tolerance
        && std::abs(first_max_x - second_max_x) <= tolerance
        && std::abs(first_max_y - second_max_y) <= tolerance
        && std::abs(first_max_z - second_max_z) <= tolerance;
}

double commonArea(const TopoDS_Face& first, const TopoDS_Face& second, double tolerance)
{
    try {
        BRepAlgoAPI_Common common(first, second);
        common.SetFuzzyValue(tolerance);
        common.Build();
        if (!common.IsDone() || common.Shape().IsNull())
            return 0.0;
        GProp_GProps properties;
        BRepGProp::SurfaceProperties(common.Shape(), properties);
        return std::abs(properties.Mass());
    } catch (const Standard_Failure&) {
        return 0.0;
    }
}

/**
 * @brief 判断两个独立 Face 是否在 OCC 数值精度内覆盖相同区域。
 */
bool areDuplicateFaces(const FaceMetrics& first, const FaceMetrics& second, double tolerance)
{
    if (first.surface_type != second.surface_type
        || !boxesMatch(first.box, second.box, tolerance)
        || first.center.Distance(second.center) > tolerance
        || !nearlyEqual(first.area, second.area, tolerance * tolerance)
        || !nearlyEqual(first.perimeter, second.perimeter, tolerance)) {
        return false;
    }

    try {
        BRepExtrema_DistShapeShape distance(first.face, second.face);
        distance.Perform();
        if (!distance.IsDone() || distance.Value() > tolerance)
            return false;

        // 对真正接触的同域面再比较公共区域，排除仅有相同包围盒和几何量的交叉面。
        if (distance.Value() <= Precision::Confusion()) {
            const double overlap_area = commonArea(first.face, second.face, tolerance);
            if (!nearlyEqual(overlap_area, first.area, tolerance * tolerance)
                || !nearlyEqual(overlap_area, second.area, tolerance * tolerance)) {
                return false;
            }
        }
        return true;
    } catch (const Standard_Failure&) {
        return false;
    }
}

/**
 * @brief 使用预先收集的 Edge 判断两面是否属于正常拓扑相邻关系。
 */
bool shareTopologicalEdge(const FaceMetrics& first, const FaceMetrics& second)
{
    const auto& fewer_edges = first.edges.size() <= second.edges.size()
        ? first.edges
        : second.edges;
    const auto& more_edges = first.edges.size() <= second.edges.size()
        ? second.edges
        : first.edges;
    for (const TopoDS_Edge& first_edge : fewer_edges) {
        if (std::any_of(more_edges.begin(), more_edges.end(), [&first_edge](const TopoDS_Edge& second_edge) {
                return first_edge.IsSame(second_edge);
            })) {
            return true;
        }
    }
    return false;
}

/**
 * @brief 判断 Section 边是否进入至少一张面的内部，排除双方都在边界上的相邻接触。
 */
bool entersFaceInterior(
    const TopoDS_Edge& edge,
    const TopoDS_Face& first,
    const TopoDS_Face& second,
    double tolerance)
{
    BRepAdaptor_Curve curve(edge);
    const double first_parameter = curve.FirstParameter();
    const double last_parameter = curve.LastParameter();
    if (!std::isfinite(first_parameter) || !std::isfinite(last_parameter)
        || first_parameter >= last_parameter) {
        return false;
    }

    // 避开端点，只判断相交边内部；真正穿过时至少有一张 Face 的分类为 IN。
    for (double ratio : { 0.25, 0.5, 0.75 }) {
        const gp_Pnt point = curve.Value(
            first_parameter + (last_parameter - first_parameter) * ratio);
        BRepClass_FaceClassifier first_classifier(first, point, tolerance, true);
        BRepClass_FaceClassifier second_classifier(second, point, tolerance, true);
        const TopAbs_State first_state = first_classifier.State();
        const TopAbs_State second_state = second_classifier.State();
        const bool on_first = first_state == TopAbs_IN || first_state == TopAbs_ON;
        const bool on_second = second_state == TopAbs_IN || second_state == TopAbs_ON;
        if (on_first && on_second
            && (first_state == TopAbs_IN || second_state == TopAbs_IN)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Section 结果对两张 Face 关系给出的判定能力。
 */
enum class FacePairRelation {
    //! Section 未产生任何 Edge：两面不可能共享 2D 区域，兜底 Common 可以跳过。
    Separated,
    //! 存在进入至少一张 Face 内部的交线。
    Crossing,
    //! 只有落在边界上的交线，或 Section 失败，需要后续兜底判定。
    Inconclusive,
};

/**
 * @brief 判断经过 Face 边界修剪后的 Section 结果中两面所处的几何关系。
 *
 * 两面共享 2D 区域时支撑曲面必然在重合区域内相同，而该重合区域的边界同样属于
 * Section 结果，因此 Section 完全没有 Edge 就足以排除同域重叠和内部穿插。
 */
FacePairRelation classifyFacePair(
    const TopoDS_Face& first,
    const TopoDS_Face& second,
    double tolerance)
{
    try {
        // IntTools_FaceFace 的中间曲线可能包含几何共边，必须用 Section 完成边界修剪。
        BRepAlgoAPI_Section section(first, second, false);
        section.SetFuzzyValue(tolerance);
        section.Build();
        if (!section.IsDone())
            return FacePairRelation::Inconclusive;

        bool has_edge = false;
        for (TopExp_Explorer edge(section.Shape(), TopAbs_EDGE); edge.More(); edge.Next()) {
            has_edge = true;
            if (entersFaceInterior(
                    TopoDS::Edge(edge.Current()), first, second, tolerance)) {
                return FacePairRelation::Crossing;
            }
        }
        return has_edge ? FacePairRelation::Inconclusive : FacePairRelation::Separated;
    } catch (const Standard_Failure&) {
        return FacePairRelation::Inconclusive;
    }
}

/**
 * @brief 判断两张非平面 Face 是否可能覆盖同一片曲面区域。
 */
bool mayHaveCommonArea(const FaceMetrics& first, const FaceMetrics& second, double tolerance)
{
    const bool first_analytic = first.surface_type == GeomAbs_Plane
        || first.surface_type == GeomAbs_Cylinder
        || first.surface_type == GeomAbs_Cone
        || first.surface_type == GeomAbs_Sphere
        || first.surface_type == GeomAbs_Torus;
    const bool second_analytic = second.surface_type == GeomAbs_Plane
        || second.surface_type == GeomAbs_Cylinder
        || second.surface_type == GeomAbs_Cone
        || second.surface_type == GeomAbs_Sphere
        || second.surface_type == GeomAbs_Torus;
    if (first_analytic && second_analytic && first.surface_type != second.surface_type)
        return false;

    if (first.surface_type == GeomAbs_Cylinder
        && second.surface_type == GeomAbs_Cylinder) {
        const gp_Ax1& first_axis = first.cylinder.Axis();
        const gp_Ax1& second_axis = second.cylinder.Axis();
        return first_axis.Direction().IsParallel(
                   second_axis.Direction(), Precision::Angular())
            && gp_Lin(first_axis).Distance(gp_Lin(second_axis)) <= tolerance
            && nearlyEqual(first.cylinder.Radius(), second.cylinder.Radius(), tolerance);
    }
    return true;
}

bool facesIntersect(
    const FaceMetrics& first,
    const FaceMetrics& second,
    double tolerance)
{
    Bnd_Box first_box = first.box;
    Bnd_Box second_box = second.box;
    first_box.Enlarge(tolerance);
    second_box.Enlarge(tolerance);
    if (first_box.IsOut(second_box) || shareTopologicalEdge(first, second))
        return false;

    if (first.surface_type == GeomAbs_Plane && second.surface_type == GeomAbs_Plane) {
        const bool parallel = first.plane.Axis().Direction().IsParallel(
            second.plane.Axis().Direction(), Precision::Angular());
        if (parallel) {
            if (first.plane.Distance(second.plane) > tolerance)
                return false;
            // 共面重叠（含贴合）算相交：与前处理器把 overlapping surfaces 计入
            // intersection 的口径一致。
            return commonArea(first.face, second.face, tolerance) > tolerance * tolerance;
        }
        return classifyFacePair(first.face, second.face, tolerance)
            == FacePairRelation::Crossing;
    }

    const FacePairRelation relation = classifyFacePair(first.face, second.face, tolerance);
    if (relation == FacePairRelation::Crossing)
        return true;
    // Section 没有任何交线时两面既不可能内部穿插，也不可能共享 2D 区域，
    // 无需再执行昂贵的同域公共面积布尔运算。
    if (relation == FacePairRelation::Separated)
        return false;
    return mayHaveCommonArea(first, second, tolerance)
        && commonArea(first.face, second.face, tolerance) > tolerance * tolerance;
}

/**
 * @brief 检查单张 Face 的边界是否存在自交或内外轮廓互相穿插。
 *
 * HyperMesh 的 Self Intersections 同时覆盖单 Surface 自交。OCCT 将这类拓扑错误分别
 * 记录为 Wire 自交和 Face 内多个 Wire 相交，这里只提取这两种状态，避免把其他无效
 * 拓扑错误混入 Self Intersections 类别。
 */
bool faceHasSelfIntersection(
    const BRepCheck_Analyzer& analyzer, const TopoDS_Face& face)
{
    try {
        const occ::handle<BRepCheck_Result>& face_result = analyzer.Result(face);
        if (!face_result.IsNull()) {
            for (NCollection_List<BRepCheck_Status>::Iterator status(face_result->Status());
                status.More(); status.Next()) {
                if (status.Value() == BRepCheck_IntersectingWires)
                    return true;
            }
        }
        for (TopExp_Explorer wire(face, TopAbs_WIRE); wire.More(); wire.Next()) {
            const occ::handle<BRepCheck_Result>& wire_result = analyzer.Result(wire.Current());
            if (wire_result.IsNull())
                continue;
            for (NCollection_List<BRepCheck_Status>::Iterator status(wire_result->Status());
                status.More(); status.Next()) {
                if (status.Value() == BRepCheck_SelfIntersectingWire)
                    return true;
            }
        }
    } catch (const Standard_Failure&) {
        return false;
    }
    return false;
}

/**
 * @brief 检查协作式取消标记；置位时抛出取消异常。
 */
void throwIfCancelled(const std::atomic<bool>* cancel)
{
    if (cancel != nullptr && cancel->load(std::memory_order_relaxed))
        throw GeometryTopologyDiagnosticCancelled("geometry topology diagnosis cancelled");
}

class ParallelFailureCollector {
public:
    explicit ParallelFailureCollector(size_t count)
        : failures_(count)
    {
    }

    //! 记录当前正在处理的异常；由并行体在 catch(...) 中调用。
    void capture(size_t index)
    {
        failures_[index] = std::current_exception();
    }

    //! 串行阶段调用：存在异常时按索引升序抛出第一个。
    void rethrowFirst() const
    {
        for (const std::exception_ptr& failure : failures_) {
            if (failure)
                std::rethrow_exception(failure);
        }
    }

private:
    std::vector<std::exception_ptr> failures_;
};
}

namespace {

//! 面片扫描的网格偏置取模型包围盒对角线的这个比例。实测甜点约 4e-5（约为显示用
//! 剖分密度的 3～4 倍）：再粗则剪不掉多少候选对，再细则建网格与扫描本身开始超收益。
constexpr double kFaceSweepDeflectionRatio = 4.0e-5;
//! 扫描容差相对网格偏置的倍数。必须不小于 2：设三角形到真实曲面距离不超过偏置 δ，
//! 两面若有公共点 p，则 p 到两张三角形集合的距离都小于 δ，故两集合距离小于 2δ；
//! 取反即「两集合距离大于容差且容差大于等于 2δ，则两面无公共点」。多出的 0.5 用来
//! 覆盖网格生成器只按采样点校验偏置的误差。
constexpr double kFaceSweepToleranceFactor = 2.5;
//! 三角剖分的弦高容差（弧度），与界面显示用的剖分保持一致。
constexpr double kFaceSweepAngularDeflection = 0.5;
//! 用于估计精确判定总代价的探测窗口（对数）。每处理这么多对就用
//! 「已花时间 / 已处理对数」外推一次总代价。
constexpr size_t kFaceSweepProbePairs = 32;
//! 只有当外推出的精确判定总代价超过这个毫秒数时，才值得付一次全局面片扫描。
//! 取 200 是实测结果：Self Intersections 收窄到同一 Solid 后，中等规模模型（254 面 / 237 个候选对）
//! 用宽相位也是净收益（418 -> 259 ms），门槛设得更高只会让它们白白错过；而 6～13 面的小
//! 模型外推代价本就低于这个值，不会为一次三角化付钱。
constexpr double kFaceSweepMinimumSavingMs = 200.0;

/**
 * @brief 将无序 Face 对归一化成稳定键，避免不同扫描阶段的遍历顺序造成漏检。
 */
constexpr std::uint64_t packFacePair(size_t first, size_t second)
{
    const size_t low = std::min(first, second);
    const size_t high = std::max(first, second);
    return (static_cast<std::uint64_t>(low) << 32) | static_cast<std::uint64_t>(high);
}

static_assert(packFacePair(3, 7) == packFacePair(7, 3), "Face pair keys must be unordered");

/**
 * @brief 面片空间扫描：一次求出「两张面在容差内可能接触」的面对白名单。
 *
 * 逐对 BRepAlgoAPI_Section 的代价主要由每一对都要重建一遍的拓扑准备（BOPAlgo_PaveFiller）
 * 决定。这里换成成熟前处理器（如 HyperMesh 的 Geometry Interference Check）的做法：
 * 把 Shape 浅拷贝一份、按相对偏置三角化，再用 OCC 自带的 BVH 自干涉扫描一次求出全部
 * 相互重叠的面对，从而把「重复的拓扑准备」替换成「一次全局空间扫描」。
 *
 * 保守性由 kFaceSweepToleranceFactor 的推导保证：**只有被扫描报告的对才继续走精确判定，
 * 未被报告的对被证明相距超过容差**。因此它只可能让判定变慢，不可能改变判定结论。
 * 若扫描无法可靠建立（拷贝、网格或扫描任一失败），一律退化为逐对精确判定。
 */
class FaceProximitySweep {
public:
    //! 构建失败时返回 nullptr，调用方退化为逐对精确判定。
    static std::unique_ptr<FaceProximitySweep> build(const TopoDS_Shape& root,
        const std::vector<FaceMetrics>& faces, const std::atomic<bool>* cancel)
    {
        throwIfCancelled(cancel);
        if (faces.size() < 2)
            return nullptr;

        const double diagonal = facesDiagonal(faces);
        if (!std::isfinite(diagonal) || diagonal <= 0.0)
            return nullptr;
        const double deflection = diagonal * kFaceSweepDeflectionRatio;
        const double tolerance = deflection * kFaceSweepToleranceFactor;
        if (!std::isfinite(deflection) || deflection <= 0.0)
            return nullptr;

        try {
            // 浅拷贝：与原件共享几何，只多一份三角剖分。这样不会覆盖界面上已经用来
            // 显示的剖分（本函数只用于诊断，不应改变外观或显示精度）。
            BRepBuilderAPI_Copy copier(root, Standard_False, Standard_False);
            const TopoDS_Shape swept_shape = copier.Shape();
            if (swept_shape.IsNull())
                return nullptr;

            // 面索引对应关系用拷贝器自己的映射取，不依赖遍历顺序。
            std::vector<TopoDS_Face> swept_faces;
            swept_faces.reserve(faces.size());
            for (const FaceMetrics& metrics : faces) {
                // Modified() 返回的是列表：一个面最多可能被拆成多个结果，这里要求一一对应。
                const NCollection_List<TopoDS_Shape>& copied = copier.Modified(metrics.face);
                if (copied.Extent() != 1)
                    return nullptr;
                const TopoDS_Shape& copied_face = copied.First();
                if (copied_face.IsNull() || copied_face.ShapeType() != TopAbs_FACE)
                    return nullptr;
                swept_faces.push_back(TopoDS::Face(copied_face));
            }

            BRepMesh_IncrementalMesh mesher(
                swept_shape, deflection, Standard_False, kFaceSweepAngularDeflection, Standard_True);
            if (!mesher.IsDone())
                return nullptr;
            // 扫描器依赖每张面的三角网格；部分剖分成功也不能作为排除面对的依据。
            for (const TopoDS_Face& face : swept_faces) {
                TopLoc_Location location;
                const occ::handle<Poly_Triangulation>& triangulation
                    = BRep_Tool::Triangulation(face, location);
                if (triangulation.IsNull())
                    return nullptr;
            }

            BRepExtrema_SelfIntersection scanner(swept_shape);
            scanner.SetTolerance(tolerance);
            scanner.Perform();
            if (!scanner.IsDone())
                return nullptr;

            NCollection_DataMap<TopoDS_Shape, size_t, TopTools_ShapeMapHasher> face_index;
            for (size_t index = 0; index < swept_faces.size(); ++index)
                face_index.Bind(swept_faces[index], index);

            auto sweep = std::unique_ptr<FaceProximitySweep>(new FaceProximitySweep());
            for (NCollection_DataMap<int, TColStd_PackedMapOfInteger>::Iterator it(
                     scanner.OverlapElements());
                it.More(); it.Next()) {
                const size_t first = lookupFaceIndex(face_index, scanner.GetSubShape(it.Key()));
                if (first == kInvalidFaceIndex)
                    return nullptr;
                for (TColStd_PackedMapOfInteger::Iterator other(it.Value()); other.More();
                    other.Next()) {
                    const size_t second
                        = lookupFaceIndex(face_index, scanner.GetSubShape(other.Key()));
                    if (second == kInvalidFaceIndex)
                        return nullptr;
                    sweep->reported_.insert(packFacePair(first, second));
                }
            }
            return sweep;
        } catch (const Standard_Failure&) {
            return nullptr;
        } catch (const std::exception&) {
            return nullptr;
        }
    }

    //! 该对是否需要继续做精确判定；false 表示两面已被证明相距超过容差。
    bool needsExactTest(size_t first, size_t second) const
    {
        return reported_.count(packFacePair(first, second)) != 0;
    }

private:
    static constexpr size_t kInvalidFaceIndex = static_cast<size_t>(-1);

    static size_t lookupFaceIndex(
        const NCollection_DataMap<TopoDS_Shape, size_t, TopTools_ShapeMapHasher>& face_index,
        const TopoDS_Face& face)
    {
        return face_index.IsBound(face) ? face_index.Find(face) : kInvalidFaceIndex;
    }

    //! 模型包围盒对角线，取自逐面测量阶段已经算好的包围盒，因此是零成本。
    static double facesDiagonal(const std::vector<FaceMetrics>& faces)
    {
        std::array<double, 3> minimum { std::numeric_limits<double>::max(),
            std::numeric_limits<double>::max(), std::numeric_limits<double>::max() };
        std::array<double, 3> maximum { std::numeric_limits<double>::lowest(),
            std::numeric_limits<double>::lowest(), std::numeric_limits<double>::lowest() };
        for (const FaceMetrics& metrics : faces) {
            for (size_t axis = 0; axis < 3; ++axis) {
                minimum[axis] = std::min(minimum[axis], metrics.minimum[axis]);
                maximum[axis] = std::max(maximum[axis], metrics.maximum[axis]);
            }
        }
        const double dx = maximum[0] - minimum[0];
        const double dy = maximum[1] - minimum[1];
        const double dz = maximum[2] - minimum[2];
        if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz))
            return -1.0;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    std::unordered_set<std::uint64_t> reported_;
};

/**
 * @brief GeometryTopologyDiagnosticSession 的实现。
 *
 * 把 diagnoseTopology() 的三段工作（分类 Edge、测量 Face、逐对判定）拆成可按时间片推进的
 * 步骤。每一步的处理顺序与判据都与一次性实现逐行一致，因此"分片推进"与"一次跑完"得到
 * 的结果完全相同；分片只是为了能在片与片之间把工作线程让给更紧急的请求。
 */
class DiagnosticSessionImpl final : public GeometryTopologyDiagnosticSession {
public:
    DiagnosticSessionImpl(const TopoDS_Shape& root, double small_edge_length_threshold,
        double small_face_area_threshold, const GeometryTopologyDiagnosticOptions& options,
        const std::atomic<bool>* cancel)
        : root_(root)
        , small_edge_length_threshold_(small_edge_length_threshold)
        , small_face_area_threshold_(small_face_area_threshold)
        , options_(options)
        , cancel_(cancel)
    {
        validate();
    }

    bool advance(double budget_ms) override
    {
        const auto deadline = std::isfinite(budget_ms)
            ? std::chrono::steady_clock::now()
                + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<double, std::milli>(budget_ms))
            : std::chrono::steady_clock::time_point::max();
        while (step_ != Step::Done) {
            if (!runCurrentStep(deadline))
                return false; // 时间预算用尽，下次接着推进
            if (std::chrono::steady_clock::now() >= deadline)
                return step_ == Step::Done;
        }
        return true;
    }

    GeometryTopologyDiagnosticResult takeResult() override { return std::move(result_); }

private:
    //! 推进阶段；顺序与一次性实现的执行顺序一致。
    enum class Step {
        Edges,
        Faces,
        PairCandidates,
        Pairs,
        DuplicateGroups,
        InvalidTopology,
        Done
    };

    bool needsFacePairs() const
    {
        return options_.duplicate_faces || options_.self_intersecting_faces
            || options_.interfering_faces;
    }

    void validate()
    {
        if (root_.IsNull())
            throw std::invalid_argument("Geometry root must not be null");
        if (!std::isfinite(small_edge_length_threshold_) || small_edge_length_threshold_ <= 0.0)
            throw std::invalid_argument("Small edge length threshold must be greater than zero");
        if (!std::isfinite(small_face_area_threshold_) || small_face_area_threshold_ <= 0.0)
            throw std::invalid_argument("Small face area threshold must be greater than zero");
        throwIfCancelled(cancel_);
    }

    //! 推进当前阶段；返回 false 表示时间预算用尽、该阶段尚未完成。
    bool runCurrentStep(const std::chrono::steady_clock::time_point& deadline)
    {
        switch (step_) {
        case Step::Edges:
            return runEdges(deadline);
        case Step::Faces:
            runFaces();
            step_ = Step::PairCandidates;
            return true;
        case Step::PairCandidates:
            runPairCandidates();
            step_ = Step::Pairs;
            return true;
        case Step::Pairs:
            return runPairs(deadline);
        case Step::DuplicateGroups:
            runDuplicateGroups();
            step_ = Step::InvalidTopology;
            return true;
        case Step::InvalidTopology:
            // 与一次性实现一致：这一步之前无条件检查一次取消标记。
            throwIfCancelled(cancel_);
            runInvalidTopology();
            step_ = Step::Done;
            return true;
        default:
            return true;
        }
    }

    //! 按相邻 Face 数量分类全部 Edge，规则与网格拓扑诊断保持一致。
    bool runEdges(const std::chrono::steady_clock::time_point& deadline)
    {
        if (!options_.edge_topology && !options_.small_edges) {
            step_ = Step::Faces;
            return true;
        }
        if (edges_.IsEmpty()) {
            TopExp::MapShapesAndAncestors(root_, TopAbs_EDGE, TopAbs_FACE, edge_faces_);
            TopExp::MapShapes(root_, TopAbs_EDGE, edges_);
        }

        const int total = edges_.Extent();
        for (; edge_cursor_ <= total; ++edge_cursor_) {
            // 协作式取消与时间片检查都放在每 256 条边处，单次几何计算不中断。
            if ((edge_cursor_ & 0xFF) == 1) {
                throwIfCancelled(cancel_);
                if (std::chrono::steady_clock::now() >= deadline)
                    return false;
            }
            const TopoDS_Edge edge = TopoDS::Edge(edges_.FindKey(edge_cursor_));
            if (options_.small_edges) {
                GProp_GProps properties;
                BRepGProp::LinearProperties(edge, properties);
                if (std::abs(properties.Mass()) <= small_edge_length_threshold_)
                    result_.small_edges.push_back(edge);
            }
            if (!options_.edge_topology)
                continue;
            const int face_count = edge_faces_.Contains(edge)
                ? uniqueFaceCount(edge_faces_.FindFromKey(edge))
                : 0;
            if (face_count == 0)
                result_.isolated_edges.push_back(edge);
            else if (face_count == 1)
                result_.boundary_edges.push_back(edge);
            else if (face_count >= 3)
                result_.non_manifold_edges.push_back(edge);
        }
        step_ = Step::Faces;
        return true;
    }

    //! 逐面几何量互不依赖，先并行测量再按面顺序过滤，保证 faces 与 small_faces 的顺序和
    //! 内容与串行实现完全相同。
    void runFaces()
    {
        if (!needsFacePairs() && !options_.small_faces)
            return;

        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> face_map;
        TopExp::MapShapes(root_, TopAbs_FACE, face_map);
        const size_t face_count = static_cast<size_t>(face_map.Extent());

        std::vector<TopoDS_Face> measured_faces(face_count);
        std::vector<FaceMetrics> measured_metrics(face_count);
        std::vector<char> measurement_valid(face_count, 0);
        for (size_t face_index = 0; face_index < face_count; ++face_index) {
            measured_faces[face_index]
                = TopoDS::Face(face_map.FindKey(static_cast<int>(face_index) + 1));
        }

        ParallelFailureCollector measurement_failures(face_count);
        std::vector<size_t> face_indices(face_count);
        std::iota(face_indices.begin(), face_indices.end(), size_t { 0 });
        std::for_each(std::execution::par, face_indices.begin(), face_indices.end(),
            [&measured_faces, &measured_metrics, &measurement_valid, &measurement_failures,
                this](size_t index) {
                // 并行体内不能抛异常（会 terminate），取消只做跳过，循环后统一抛出。
                if (cancel_ != nullptr && cancel_->load(std::memory_order_relaxed))
                    return;
                try {
                    measured_metrics[index] = measureFace(measured_faces[index]);
                    measurement_valid[index] = 1;
                } catch (const Standard_Failure&) {
                    // 无法取得基本几何量的 Face 交由无效拓扑诊断处理。
                } catch (...) {
                    measurement_failures.capture(index);
                }
            });
        measurement_failures.rethrowFirst();
        throwIfCancelled(cancel_);

        // 一次分析整棵拓扑，再读取各 Face/Wire 状态，避免为每张面重复构建 Analyzer。
        std::unique_ptr<BRepCheck_Analyzer> self_intersection_analyzer;
        if (options_.self_intersecting_faces) {
            try {
                self_intersection_analyzer = std::make_unique<BRepCheck_Analyzer>(root_);
            } catch (const Standard_Failure&) {
                // Analyzer 失败时仍继续执行面对求交；单 Surface 自交由无效拓扑诊断兜底。
            }
        }

        faces_.reserve(face_count);
        for (size_t face_index = 0; face_index < face_count; ++face_index) {
            if (measurement_valid[face_index] == 0)
                continue;
            if (options_.small_faces
                && measured_metrics[face_index].area <= small_face_area_threshold_) {
                result_.small_faces.push_back(measured_faces[face_index]);
            }
            // 单 Surface 自交没有第二张 Face，不会进入后续面对循环，必须在这里单独记录。
            if (self_intersection_analyzer
                && faceHasSelfIntersection(
                    *self_intersection_analyzer, measured_faces[face_index])) {
                result_.self_intersecting_face_pairs.push_back(
                    { measured_faces[face_index], measured_faces[face_index] });
            }
            faces_.push_back(std::move(measured_metrics[face_index]));
        }
    }

    /**
     * @brief 记录每张 Face 的真实 Solid 祖先，供两套检查各自确定范围。
     *
     * Self Intersections 只比较同一 Solid 内的 Face；Geometry Interference Check 比较
     * 不同 Solid 或自由 Surface。这里不再根据包围盒关系猜测或合并 Part 身份。
     */
    void buildFaceOwners()
    {
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> solid_map;
        TopExp::MapShapes(root_, TopAbs_SOLID, solid_map);
        NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
            TopTools_ShapeMapHasher>
            face_solids;
        TopExp::MapShapesAndAncestors(root_, TopAbs_FACE, TopAbs_SOLID, face_solids);
        face_owner_solids_.assign(faces_.size(), {});
        for (size_t index = 0; index < faces_.size(); ++index) {
            const TopoDS_Face& face = faces_[index].face;
            if (!face_solids.Contains(face))
                continue;
            for (NCollection_List<TopoDS_Shape>::Iterator it(face_solids.FindFromKey(face));
                it.More(); it.Next()) {
                face_owner_solids_[index].push_back(solid_map.FindIndex(it.Value()) - 1);
            }
        }
    }

    //! @brief 两张面是否属于同一个真实 Solid；自由 Surface 之间不视为同一实体。
    bool belongsToSameSolid(size_t first, size_t second) const
    {
        const std::vector<int>& first_solids = face_owner_solids_[first];
        const std::vector<int>& second_solids = face_owner_solids_[second];
        return std::any_of(first_solids.begin(), first_solids.end(),
            [&second_solids](int solid) {
                return std::find(second_solids.begin(), second_solids.end(), solid)
                    != second_solids.end();
            });
    }

    //! 并查集将两两相同的面归并为稳定的重复面组；两者都在本阶段一次性备好。
    void runPairCandidates()
    {
        if (!needsFacePairs())
            return;
        buildFaceOwners();
        duplicate_parents_.resize(faces_.size());
        std::iota(duplicate_parents_.begin(), duplicate_parents_.end(), size_t { 0 });
        candidate_pairs_ = overlappingFacePairs(faces_, Precision::Confusion());
    }

    bool runPairs(const std::chrono::steady_clock::time_point& deadline)
    {
        if (needsFacePairs()) {
            const double tolerance = Precision::Confusion();
            if (pair_cursor_ == 0)
                pair_started_at_ = std::chrono::steady_clock::now();
            while (pair_cursor_ < candidate_pairs_.size()) {
                maybeBuildFaceSweep();
                const size_t first = candidate_pairs_[pair_cursor_].first;
                const size_t second = candidate_pairs_[pair_cursor_].second;
                throwIfCancelled(cancel_);
                // 两套检查使用独立范围：Self Intersections 只看同一 Solid，Geometry
                // Interference Check 只看不同 Solid 或自由 Surface，不再按推测 Part 二分。
                const bool same_solid = belongsToSameSolid(first, second);
                const bool want_self_intersection = same_solid && options_.self_intersecting_faces;
                const bool want_interference = !same_solid && options_.interfering_faces;
                bool separated = false;
                if (want_self_intersection || want_interference) {
                    // 全局面片扫描已证明两面相距超过容差：既不可能相交，也不可能同域重叠。
                    const bool swept_apart
                        = sweep_ != nullptr && !sweep_->needsExactTest(first, second);
                    // 共享拓扑边只能排除普通的相交判定，不能在这里跳过重复面判定：
                    // 两张重复面可能复用同一组边界 Edge。facesIntersect 会自行排除正常邻接面。
                    separated = swept_apart
                        || facesSeparated(faces_[first], faces_[second], tolerance);
                }
                if (!separated) {
                    // 重复面与穿插判定互斥；即使不显示重复面，也要先排除重复面误报。
                    if (areDuplicateFaces(faces_[first], faces_[second], tolerance)) {
                        if (options_.duplicate_faces) {
                            duplicate_pairs_.emplace_back(first, second);
                            mergeDuplicateGroups(first, second);
                        }
                    } else if ((want_self_intersection || want_interference)
                        && facesIntersect(faces_[first], faces_[second], tolerance)) {
                        // 同一 Solid 内的面对归 Self Intersections；不同 Solid 或含自由
                        // Surface 的归 Geometry Interference Check。两者判据一致：只要交线
                        // 进入面的内部就算——重叠（含共面/贴合）与穿越都算，与前处理器
                        // "any overlap between solids registers as intersections" 相符。
                        if (same_solid) {
                            result_.self_intersecting_face_pairs.push_back(
                                { faces_[first].face, faces_[second].face });
                        } else {
                            result_.interfering_face_pairs.push_back(
                                { faces_[first].face, faces_[second].face });
                        }
                    }
                }
                ++pair_cursor_;
                // 时间片检查放在一对处理完之后：单次 OCC 运算本身不打断。
                if (std::chrono::steady_clock::now() >= deadline
                    && pair_cursor_ < candidate_pairs_.size()) {
                    return false; // 预算用尽且还有剩余对
                }
            }
        }
        step_ = Step::DuplicateGroups;
        return true;
    }

    /**
     * @brief 精确判定贵到值得先做一次全局面片扫描时，把它建起来并用于后续所有对。
     *
     * 是否值得完全由实测决定：用「已处理对数 / 已花时间」外推精确判定的总代价，只有
     * 外推值超过 kFaceSweepMinimumSavingMs 才建扫描。这样小负载模型（例如 237 个候选对、
     * 整个逐对循环只要几百毫秒）不会被卷入一次代价相当的宽相位，而真正昂贵的模型会在
     * 前 kFaceSweepProbePairs 对之内就被识别出来。构建失败时退化为逐对精确判定。
     */
    void maybeBuildFaceSweep()
    {
        if (sweep_ != nullptr || sweep_skipped_)
            return;
        if (candidate_pairs_.size() <= kFaceSweepProbePairs
            || pair_cursor_ < next_sweep_probe_) {
            return;
        }
        next_sweep_probe_ = pair_cursor_ + kFaceSweepProbePairs;

        const double elapsed_ms = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - pair_started_at_)
                                      .count();
        const double projected_ms = elapsed_ms / static_cast<double>(pair_cursor_)
            * static_cast<double>(candidate_pairs_.size());
        if (projected_ms < kFaceSweepMinimumSavingMs)
            return;

        sweep_ = FaceProximitySweep::build(root_, faces_, cancel_);
        // 建不起来就不要再反复尝试，直接按原有逐对路径跑完。
        sweep_skipped_ = sweep_ == nullptr;
    }

    //! 并查集查找；带路径压缩，因此不能是 const 成员函数。
    size_t findDuplicateRoot(size_t index)
    {
        while (duplicate_parents_[index] != index) {
            duplicate_parents_[index] = duplicate_parents_[duplicate_parents_[index]];
            index = duplicate_parents_[index];
        }
        return index;
    }

    void mergeDuplicateGroups(size_t first, size_t second)
    {
        first = findDuplicateRoot(first);
        second = findDuplicateRoot(second);
        if (first != second)
            duplicate_parents_[second] = first;
    }

    void runDuplicateGroups()
    {
        if (!options_.duplicate_faces)
            return;
        std::unordered_map<size_t, size_t> group_indices;
        for (const auto& [first, second] : duplicate_pairs_) {
            const size_t parent = findDuplicateRoot(first);
            auto [group_it, inserted]
                = group_indices.emplace(parent, result_.duplicate_face_groups.size());
            if (inserted)
                result_.duplicate_face_groups.emplace_back();
            auto& group_faces = result_.duplicate_face_groups[group_it->second].faces;
            auto append_unique = [this, &group_faces](size_t index) {
                if (std::none_of(group_faces.begin(), group_faces.end(),
                        [this, index](const TopoDS_Face& face) {
                            return face.IsSame(faces_[index].face);
                        })) {
                    group_faces.push_back(faces_[index].face);
                }
            };
            append_unique(first);
            append_unique(second);
        }
    }

    void runInvalidTopology()
    {
        if (!options_.invalid_topology)
            return;
        // Analyzer 已包含根形状及其全部子形状的检查结果，后续直接复用，避免逐形状重建。
        const BRepCheck_Analyzer analyzer(root_);
        if (analyzer.IsValid())
            return;
        for (TopAbs_ShapeEnum type : { TopAbs_FACE, TopAbs_WIRE, TopAbs_EDGE, TopAbs_VERTEX }) {
            for (TopExp_Explorer subshape(root_, type); subshape.More(); subshape.Next()) {
                if (!analyzer.IsValid(subshape.Current()))
                    result_.invalid_shapes.push_back(subshape.Current());
            }
        }
        if (result_.invalid_shapes.empty())
            result_.invalid_shapes.push_back(root_);
    }

    // 输入
    TopoDS_Shape root_;
    double small_edge_length_threshold_ { 0.0 };
    double small_face_area_threshold_ { 0.0 };
    GeometryTopologyDiagnosticOptions options_ {};
    const std::atomic<bool>* cancel_ { nullptr };

    Step step_ { Step::Edges };

    // Edge 分类阶段
    NCollection_IndexedDataMap<TopoDS_Shape, NCollection_List<TopoDS_Shape>,
        TopTools_ShapeMapHasher>
        edge_faces_;
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edges_;
    int edge_cursor_ { 1 };

    // Face 测量阶段
    std::vector<FaceMetrics> faces_;

    // 逐对判定阶段
    std::vector<size_t> duplicate_parents_;
    std::vector<std::pair<size_t, size_t>> candidate_pairs_;
    size_t pair_cursor_ { 0 };
    //! 全局一次的面片空间扫描；nullptr 表示本次不使用（未启用或构建失败）。
    std::unique_ptr<FaceProximitySweep> sweep_;
    //! 扫描构建失败后不再重试，按原有逐对路径跑完。
    bool sweep_skipped_ { false };
    //! 逐对阶段开始时刻，用于外推总代价。
    std::chrono::steady_clock::time_point pair_started_at_ {};
    //! 每张 Face 的真实 Solid 祖先（`buildFaceOwners` 的 solid_map 下标）；空表示自由 Surface。
    std::vector<std::vector<int>> face_owner_solids_;
    //! 下一次做代价外推的候选对下标。
    size_t next_sweep_probe_ { kFaceSweepProbePairs };
    std::vector<std::pair<size_t, size_t>> duplicate_pairs_;

    GeometryTopologyDiagnosticResult result_;
};

} // namespace

std::unique_ptr<GeometryTopologyDiagnosticSession> GeometryTopologyDiagnosticSession::start(
    const TopoDS_Shape& root,
    double small_edge_length_threshold,
    double small_face_area_threshold,
    const GeometryTopologyDiagnosticOptions& options,
    const std::atomic<bool>* cancel)
{
    return std::make_unique<DiagnosticSessionImpl>(root, small_edge_length_threshold,
        small_face_area_threshold, options, cancel);
}

GeometryTopologyDiagnosticResult GeometryTopologyEditor::diagnoseTopology(
    const TopoDS_Shape& root,
    double small_edge_length_threshold,
    double small_face_area_threshold,
    const GeometryTopologyDiagnosticOptions& options,
    const std::atomic<bool>* cancel)
{
    // 与分片推进共用同一套实现：非有限预算表示一次推进到完成。
    const std::unique_ptr<GeometryTopologyDiagnosticSession> session
        = GeometryTopologyDiagnosticSession::start(root, small_edge_length_threshold,
            small_face_area_threshold, options, cancel);
    while (!session->advance(std::numeric_limits<double>::infinity())) {
    }
    return session->takeResult();
}


TopoDS_Shape GeometryTopologyEditor::removeTopLevelShape(
    const TopoDS_Shape& root,
    const TopoDS_Shape& target,
    bool delete_children)
{
    if (root.IsNull() || target.IsNull())
        throw std::invalid_argument("Geometry root and selected shape must not be null");
    if (!isSupportedShapeType(target.ShapeType()))
        throw std::invalid_argument("Only vertex, edge, face or solid can be deleted");

    std::vector<TopoDS_Shape> retained_shapes;
    bool target_found = false;

    // 阶段 1 只修改根形状或扁平根 Compound 的直接子形状，避免隐式破坏 Solid/Shell。
    if (root.IsSame(target)) {
        target_found = true;
    } else if (root.ShapeType() == TopAbs_COMPOUND) {
        for (TopoDS_Iterator it(root); it.More(); it.Next()) {
            const TopoDS_Shape& child = it.Value();
            if (child.IsSame(target)) {
                target_found = true;
                continue;
            }
            retained_shapes.push_back(child);
        }
    }

    if (!target_found)
        throw std::invalid_argument("Only a top-level independent geometry shape can be deleted");

    const TopAbs_ShapeEnum lower_type = lowerShapeType(target.ShapeType());
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> retained_lower_shapes;
    if (lower_type != TopAbs_SHAPE) {
        for (const TopoDS_Shape& shape : retained_shapes)
            TopExp::MapShapes(shape, lower_type, retained_lower_shapes);
    }

    BRep_Builder builder;
    TopoDS_Compound result;
    builder.MakeCompound(result);
    int result_child_count = 0;

    for (const TopoDS_Shape& shape : retained_shapes) {
        builder.Add(result, shape);
        ++result_child_count;
    }

    // 非级联删除时，仅提升没有被其他保留形状引用的直接下级拓扑。
    if (!delete_children && lower_type != TopAbs_SHAPE) {
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> lower_shapes;
        TopExp::MapShapes(target, lower_type, lower_shapes);
        for (int index = 1; index <= lower_shapes.Extent(); ++index) {
            const TopoDS_Shape& lower_shape = lower_shapes.FindKey(index);
            if (retained_lower_shapes.Contains(lower_shape))
                continue;
            builder.Add(result, lower_shape);
            ++result_child_count;
        }
    }

    if (result_child_count == 0)
        return {};
    if (!BRepCheck_Analyzer(result).IsValid())
        throw std::runtime_error("Deleting the geometry shape produced invalid topology");
    return result;
}
