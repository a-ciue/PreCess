#include "GeometryTopologyEditor.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepAdaptor_Surface.hxx>
#include <BRepAlgoAPI_Common.hxx>
#include <BRepAlgoAPI_Section.hxx>
#include <BRepBndLib.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepClass_FaceClassifier.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepExtrema_DistShapeShape.hxx>
#include <BRepFeat_SplitShape.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <BRep_Builder.hxx>
#include <BRepTools.hxx>
#include <BRepTools_ReShape.hxx>
#include <Bnd_Box.hxx>
#include <GeomAPI_ProjectPointOnSurf.hxx>
#include <GeomAbs_CurveType.hxx>
#include <Geom_BSplineSurface.hxx>
#include <Geom_BezierSurface.hxx>
#include <Geom_Curve.hxx>
#include <Geom_Surface.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GProp_GProps.hxx>
#include <Precision.hxx>
#include <NCollection_Array1.hxx>
#include <NCollection_Array2.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <ShapeFix_Shape.hxx>
#include <ShapeBuild_Edge.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopAbs_State.hxx>
#include <TopExp.hxx>
#include <NCollection_IndexedDataMap.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_List.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Vertex.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>
#include <gp_Cylinder.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
#include <gp_Pln.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <TopExp_Explorer.hxx>
#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <execution>
#include <numeric>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
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
 * @brief 通过曲线采样、曲面投影和面参数域分类，确认分割边已经位于目标面上。
 */
bool isEdgeOnFace(const TopoDS_Edge& edge, const TopoDS_Face& face)
{
    const occ::handle<Geom_Surface> surface = BRep_Tool::Surface(face);
    if (surface.IsNull())
        return false;

    BRepAdaptor_Curve curve(edge);
    const double first = curve.FirstParameter();
    const double last = curve.LastParameter();
    if (!std::isfinite(first) || !std::isfinite(last) || first >= last)
        return false;

    constexpr int sample_count = 9;
    const double tolerance = std::max({
        BRep_Tool::Tolerance(face),
        BRep_Tool::Tolerance(edge),
        Precision::Confusion(),
    });

    // 同时检查曲线到曲面的距离和投影点是否位于 Face 的有效参数域。
    for (int sample = 0; sample < sample_count; ++sample) {
        const double ratio = static_cast<double>(sample) / (sample_count - 1);
        const gp_Pnt point = curve.Value(first + (last - first) * ratio);
        GeomAPI_ProjectPointOnSurf projection(point, surface);
        if (projection.NbPoints() == 0 || projection.LowerDistance() > tolerance)
            return false;

        double u = 0.0;
        double v = 0.0;
        projection.LowerDistanceParameters(u, v);
        BRepClass_FaceClassifier classifier(face, gp_Pnt2d(u, v), tolerance);
        const TopAbs_State state = classifier.State();
        if (state != TopAbs_IN && state != TopAbs_ON)
            return false;
    }
    return true;
}

/**
 * @brief 统计根形状中指定类型的不重复子形状数量。
 */
int countSubshapes(const TopoDS_Shape& root, TopAbs_ShapeEnum type)
{
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> shapes;
    TopExp::MapShapes(root, type, shapes);
    return shapes.Extent();
}

/**
 * @brief 执行替换后的基础修复，并拒绝空结果或无效 BRep。
 */
TopoDS_Shape fixAndValidate(TopoDS_Shape shape, const char* operation)
{
    if (shape.IsNull())
        throw std::runtime_error(std::string(operation) + " returned an empty result");
    ShapeFix_Shape fixer(shape);
    fixer.Perform();
    shape = fixer.Shape();
    if (shape.IsNull() || !BRepCheck_Analyzer(shape).IsValid())
        throw std::runtime_error(std::string(operation) + " produced invalid topology");
    return shape;
}

/**
 * @brief 确认目标子形状属于当前根形状。
 */
void requireSubshape(
    const TopoDS_Shape& root,
    const TopoDS_Shape& shape,
    TopAbs_ShapeEnum type,
    const char* message)
{
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> shapes;
    TopExp::MapShapes(root, type, shapes);
    if (!shapes.Contains(shape))
        throw std::invalid_argument(message);
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
            std::array<double, 3> minimum = { 1e300, 1e300, 1e300 };
            std::array<double, 3> maximum = { -1e300, -1e300, -1e300 };
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
 * @brief 判断经过 Face 边界修剪后的 Section 结果中是否存在内部相交边。
 */
bool hasIntersectionEdge(const TopoDS_Face& first, const TopoDS_Face& second, double tolerance)
{
    return classifyFacePair(first, second, tolerance) == FacePairRelation::Crossing;
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
            return commonArea(first.face, second.face, tolerance) > tolerance * tolerance;
        }
        return hasIntersectionEdge(first.face, second.face, tolerance);
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
 * @brief 收集并行区间内逃逸的异常，并在串行阶段按原顺序重新抛出。
 *
 * std::execution::par 的并行算法一旦有异常逃出并行体就会直接终止进程，因此并行体
 * 必须自行捕获；只按索引升序重新抛出第一个失败，使调用方观察到的异常与串行实现一致。
 */
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

GeometryTopologyDiagnosticResult GeometryTopologyEditor::diagnoseTopology(
    const TopoDS_Shape& root,
    double small_edge_length_threshold,
    double small_face_area_threshold,
    const GeometryTopologyDiagnosticOptions& options)
{
    if (root.IsNull())
        throw std::invalid_argument("Geometry root must not be null");
    if (!std::isfinite(small_edge_length_threshold) || small_edge_length_threshold <= 0.0)
        throw std::invalid_argument("Small edge length threshold must be greater than zero");
    if (!std::isfinite(small_face_area_threshold) || small_face_area_threshold <= 0.0)
        throw std::invalid_argument("Small face area threshold must be greater than zero");

    GeometryTopologyDiagnosticResult result;

    if (options.edge_topology || options.small_edges) {
        // 按相邻 Face 数量分类全部 Edge，规则与网格拓扑诊断保持一致。
        NCollection_IndexedDataMap<TopoDS_Shape,
            NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher>
            edge_faces;
        TopExp::MapShapesAndAncestors(root, TopAbs_EDGE, TopAbs_FACE, edge_faces);
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edges;
        TopExp::MapShapes(root, TopAbs_EDGE, edges);
        for (int edge_index = 1; edge_index <= edges.Extent(); ++edge_index) {
            const TopoDS_Edge edge = TopoDS::Edge(edges.FindKey(edge_index));
            if (options.small_edges) {
                GProp_GProps properties;
                BRepGProp::LinearProperties(edge, properties);
                if (std::abs(properties.Mass()) <= small_edge_length_threshold)
                    result.small_edges.push_back(edge);
            }
            if (!options.edge_topology)
                continue;
            const int face_count = edge_faces.Contains(edge)
                ? uniqueFaceCount(edge_faces.FindFromKey(edge))
                : 0;
            if (face_count == 0)
                result.isolated_edges.push_back(edge);
            else if (face_count == 1)
                result.boundary_edges.push_back(edge);
            else if (face_count >= 3)
                result.non_manifold_edges.push_back(edge);
        }
    }

    std::vector<FaceMetrics> faces;
    const bool needs_face_pairs = options.duplicate_faces || options.intersecting_faces;
    if (needs_face_pairs || options.small_faces) {
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> face_map;
        TopExp::MapShapes(root, TopAbs_FACE, face_map);
        const size_t face_count = static_cast<size_t>(face_map.Extent());

        // 逐面几何量互不依赖，先并行测量再按面顺序过滤，保证 faces 与 small_faces
        // 的顺序和内容与串行实现完全相同。
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
            [&measured_faces, &measured_metrics, &measurement_valid, &measurement_failures](
                size_t index) {
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

        faces.reserve(face_count);
        for (size_t face_index = 0; face_index < face_count; ++face_index) {
            if (measurement_valid[face_index] == 0)
                continue;
            if (options.small_faces
                && measured_metrics[face_index].area <= small_face_area_threshold) {
                result.small_faces.push_back(measured_faces[face_index]);
            }
            faces.push_back(std::move(measured_metrics[face_index]));
        }
    }

    // 并查集将两两相同的面归并为稳定的重复面组。
    std::vector<size_t> duplicate_parents(faces.size());
    for (size_t face_index = 0; face_index < faces.size(); ++face_index)
        duplicate_parents[face_index] = face_index;
    auto find_parent = [&duplicate_parents](size_t index) {
        while (duplicate_parents[index] != index) {
            duplicate_parents[index] = duplicate_parents[duplicate_parents[index]];
            index = duplicate_parents[index];
        }
        return index;
    };
    auto merge_groups = [&duplicate_parents, &find_parent](size_t first, size_t second) {
        first = find_parent(first);
        second = find_parent(second);
        if (first != second)
            duplicate_parents[second] = first;
    };

    std::vector<std::pair<size_t, size_t>> duplicate_pairs;
    if (needs_face_pairs) {
        const double tolerance = Precision::Confusion();
        const auto candidate_pairs = overlappingFacePairs(faces, tolerance);
        for (const auto& [first, second] : candidate_pairs) {
            // 保守边界已证明两面相距超过容差时，两面既不可能相交也不可能同域重叠。
            if (facesSeparated(faces[first], faces[second], tolerance))
                continue;
            // 重复面与相交面互斥；即使不显示重复面，也要先排除重复面误报。
            if (areDuplicateFaces(faces[first], faces[second], tolerance)) {
                if (options.duplicate_faces) {
                    duplicate_pairs.emplace_back(first, second);
                    merge_groups(first, second);
                }
                continue;
            }
            if (options.intersecting_faces
                && facesIntersect(faces[first], faces[second], tolerance)) {
                result.intersecting_face_pairs.push_back(
                    { faces[first].face, faces[second].face });
            }
        }
    }
    if (options.duplicate_faces) {
        std::unordered_map<size_t, size_t> group_indices;
        for (const auto& [first, second] : duplicate_pairs) {
            const size_t parent = find_parent(first);
            auto [group_it, inserted] = group_indices.emplace(parent, result.duplicate_face_groups.size());
            if (inserted)
                result.duplicate_face_groups.emplace_back();
            auto& group_faces = result.duplicate_face_groups[group_it->second].faces;
            auto append_unique = [&group_faces, &faces](size_t index) {
                if (std::none_of(group_faces.begin(), group_faces.end(), [&faces, index](const TopoDS_Face& face) {
                        return face.IsSame(faces[index].face);
                    })) {
                    group_faces.push_back(faces[index].face);
                }
            };
            append_unique(first);
            append_unique(second);
        }
    }

    if (options.invalid_topology && !BRepCheck_Analyzer(root).IsValid()) {
        for (TopAbs_ShapeEnum type : { TopAbs_FACE, TopAbs_WIRE, TopAbs_EDGE, TopAbs_VERTEX }) {
            for (TopExp_Explorer subshape(root, type); subshape.More(); subshape.Next()) {
                if (!BRepCheck_Analyzer(subshape.Current()).IsValid())
                    result.invalid_shapes.push_back(subshape.Current());
            }
        }
        if (result.invalid_shapes.empty())
            result.invalid_shapes.push_back(root);
    }
    return result;
}

TopoDS_Shape GeometryTopologyEditor::splitEdge(
    const TopoDS_Shape& root,
    const TopoDS_Edge& edge,
    double ratio)
{
    if (root.IsNull() || edge.IsNull())
        throw std::invalid_argument("Geometry root and edge must not be null");
    if (!std::isfinite(ratio) || ratio <= 0.0 || ratio >= 1.0)
        throw std::invalid_argument("Split ratio must be between zero and one");
    requireSubshape(root, edge, TopAbs_EDGE, "Selected edge does not belong to the geometry root");

    try {
        TopoDS_Vertex start;
        TopoDS_Vertex end;
        TopExp::Vertices(edge, start, end, true);
        double first = 0.0;
        double last = 0.0;
        const occ::handle<Geom_Curve> curve = BRep_Tool::Curve(edge, first, last);
        if (start.IsNull() || end.IsNull() || curve.IsNull()
            || !std::isfinite(first) || !std::isfinite(last) || first >= last)
            throw std::invalid_argument("Selected edge has no splittable 3D curve");

        const double parameter = first + (last - first) * ratio;
        const TopoDS_Vertex middle = BRepBuilderAPI_MakeVertex(curve->Value(parameter));
        BRepBuilderAPI_MakeEdge first_builder(curve, start, middle, first, parameter);
        BRepBuilderAPI_MakeEdge second_builder(curve, middle, end, parameter, last);
        if (!first_builder.IsDone() || !second_builder.IsDone())
            throw std::runtime_error("OpenCASCADE failed to build split edges");

        BRepBuilderAPI_MakeWire wire_builder;
        wire_builder.Add(first_builder.Edge());
        wire_builder.Add(second_builder.Edge());
        if (!wire_builder.IsDone())
            throw std::runtime_error("OpenCASCADE failed to build the split wire");

        const int edge_count = countSubshapes(root, TopAbs_EDGE);
        const int vertex_count = countSubshapes(root, TopAbs_VERTEX);
        occ::handle<BRepTools_ReShape> reshaper = new BRepTools_ReShape();
        reshaper->Replace(edge, wire_builder.Wire());
        TopoDS_Shape result = fixAndValidate(reshaper->Apply(root), "Splitting the edge");
        if (countSubshapes(result, TopAbs_EDGE) != edge_count + 1
            || countSubshapes(result, TopAbs_VERTEX) != vertex_count + 1)
            throw std::runtime_error("Splitting the edge did not create two connected edges");
        return result;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        throw std::runtime_error(detail
                ? std::string("OpenCASCADE failed to split the edge: ") + detail
                : "OpenCASCADE failed to split the edge");
    }
}

TopoDS_Shape GeometryTopologyEditor::collapseEdge(
    const TopoDS_Shape& root,
    const TopoDS_Edge& edge,
    const gp_Pnt& target_position)
{
    if (root.IsNull() || edge.IsNull())
        throw std::invalid_argument("Geometry root and edge must not be null");
    if (!std::isfinite(target_position.X()) || !std::isfinite(target_position.Y())
        || !std::isfinite(target_position.Z()))
        throw std::invalid_argument("Collapse target position must be finite");
    requireSubshape(root, edge, TopAbs_EDGE, "Selected edge does not belong to the geometry root");

    try {
        TopoDS_Vertex start;
        TopoDS_Vertex end;
        TopExp::Vertices(edge, start, end, true);
        if (start.IsNull() || end.IsNull() || start.IsSame(end))
            throw std::invalid_argument("Selected edge must have two different vertices");

        const gp_Pnt start_point = BRep_Tool::Pnt(start);
        const gp_Pnt end_point = BRep_Tool::Pnt(end);
        TopoDS_Vertex destination = BRepBuilderAPI_MakeVertex(target_position);
        const double destination_tolerance = std::max({
            BRep_Tool::Tolerance(start),
            BRep_Tool::Tolerance(end),
            start_point.Distance(target_position),
            end_point.Distance(target_position),
        });
        BRep_Builder vertex_builder;
        vertex_builder.UpdateVertex(destination, destination_tolerance);

        const int edge_count = countSubshapes(root, TopAbs_EDGE);
        const int vertex_count = countSubshapes(root, TopAbs_VERTEX);
        occ::handle<BRepTools_ReShape> reshaper = new BRepTools_ReShape();
        reshaper->Remove(edge);

        ShapeBuild_Edge edge_builder;
        // 保留相邻边原有的 3D Curve 和 pcurve，仅替换其拓扑端点。
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> root_edges;
        TopExp::MapShapes(root, TopAbs_EDGE, root_edges);
        for (int edge_index = 1; edge_index <= root_edges.Extent(); ++edge_index) {
            const TopoDS_Edge adjacent_edge = TopoDS::Edge(root_edges.FindKey(edge_index));
            if (adjacent_edge.IsSame(edge))
                continue;

            TopoDS_Vertex adjacent_start;
            TopoDS_Vertex adjacent_end;
            TopExp::Vertices(adjacent_edge, adjacent_start, adjacent_end, true);
            const bool replace_start = !adjacent_start.IsNull()
                && (adjacent_start.IsSame(start) || adjacent_start.IsSame(end));
            const bool replace_end = !adjacent_end.IsNull()
                && (adjacent_end.IsSame(start) || adjacent_end.IsSame(end));
            if (!replace_start && !replace_end)
                continue;
            if (replace_start && replace_end) {
                reshaper->Remove(adjacent_edge);
                continue;
            }

            const TopoDS_Vertex new_start = replace_start ? destination : adjacent_start;
            const TopoDS_Vertex new_end = replace_end ? destination : adjacent_end;
            if (new_start.IsNull() || new_end.IsNull() || new_start.IsSame(new_end))
                throw std::runtime_error("Collapsing the edge created a degenerate adjacent edge");
            const TopoDS_Edge rebuilt_edge =
                edge_builder.CopyReplaceVertices(adjacent_edge, new_start, new_end);
            if (rebuilt_edge.IsNull())
                throw std::runtime_error("OpenCASCADE failed to rebuild an adjacent edge");
            reshaper->Replace(adjacent_edge, rebuilt_edge);
        }

        TopoDS_Shape raw_result = reshaper->Apply(root);

        // 点和边可能同时是根 Compound 的独立子节点，需要同步移除旧端点并保留目标点。
        if (raw_result.ShapeType() == TopAbs_COMPOUND) {
            NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> result_vertices;
            TopExp::MapShapes(raw_result, TopAbs_VERTEX, result_vertices);
            bool needs_top_level_destination = !result_vertices.Contains(destination);
            BRep_Builder builder;
            TopoDS_Compound compound;
            builder.MakeCompound(compound);
            bool has_destination = false;
            for (TopoDS_Iterator it(raw_result); it.More(); it.Next()) {
                const TopoDS_Shape& child = it.Value();
                if (child.ShapeType() == TopAbs_VERTEX
                    && (child.IsSame(start) || child.IsSame(end))) {
                    needs_top_level_destination = true;
                    continue;
                }
                builder.Add(compound, child);
            }
            if (needs_top_level_destination && !has_destination) {
                builder.Add(compound, destination);
                has_destination = true;
            }
            raw_result = compound;
        }
        TopoDS_Shape result = fixAndValidate(raw_result, "Collapsing the edge");
        if (countSubshapes(result, TopAbs_EDGE) != edge_count - 1
            || countSubshapes(result, TopAbs_VERTEX) != vertex_count - 1)
            throw std::runtime_error("Collapsing the edge did not remove one edge and one vertex");
        return result;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        throw std::runtime_error(detail
                ? std::string("OpenCASCADE failed to collapse the edge: ") + detail
                : "OpenCASCADE failed to collapse the edge");
    }
}

TopoDS_Vertex GeometryTopologyEditor::recommendCollapseVertex(
    const TopoDS_Shape& root,
    const TopoDS_Edge& edge)
{
    if (root.IsNull() || edge.IsNull())
        throw std::invalid_argument("Geometry root and edge must not be null");
    requireSubshape(root, edge, TopAbs_EDGE, "Selected edge does not belong to the geometry root");

    TopoDS_Vertex start;
    TopoDS_Vertex end;
    TopExp::Vertices(edge, start, end, true);
    if (start.IsNull() || end.IsNull())
        throw std::invalid_argument("Selected edge must have two vertices");

    using ShapeAncestors = NCollection_IndexedDataMap<TopoDS_Shape,
        NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher>;
    ShapeAncestors vertex_edges;
    ShapeAncestors vertex_faces;
    TopExp::MapShapesAndUniqueAncestors(root, TopAbs_VERTEX, TopAbs_EDGE, vertex_edges);
    TopExp::MapShapesAndUniqueAncestors(root, TopAbs_VERTEX, TopAbs_FACE, vertex_faces);

    const auto score = [&](const TopoDS_Vertex& vertex) {
        int curved_count = 0;
        int edge_count = 0;
        double total_length = 0.0;
        if (vertex_edges.Contains(vertex)) {
            const NCollection_List<TopoDS_Shape>& edges = vertex_edges.FindFromKey(vertex);
            for (NCollection_List<TopoDS_Shape>::Iterator it(edges); it.More(); it.Next()) {
                const TopoDS_Edge adjacent = TopoDS::Edge(it.Value());
                if (adjacent.IsSame(edge))
                    continue;
                ++edge_count;
                BRepAdaptor_Curve curve(adjacent);
                if (curve.GetType() != GeomAbs_Line)
                    ++curved_count;
                try {
                    total_length += GCPnts_AbscissaPoint::Length(curve);
                } catch (const Standard_Failure&) {
                    // 无法计算长度时仍可使用曲线、Face 和邻接数量完成稳定决策。
                }
            }
        }
        const int face_count = vertex_faces.Contains(vertex)
            ? vertex_faces.FindFromKey(vertex).Extent()
            : 0;
        return std::make_tuple(curved_count, face_count, edge_count, total_length);
    };
    return score(end) > score(start) ? end : start;
}

TopoDS_Shape GeometryTopologyEditor::mergeVertices(
    const TopoDS_Shape& root,
    const std::vector<TopoDS_Vertex>& vertices,
    const gp_Pnt& target_position)
{
    if (root.IsNull())
        throw std::invalid_argument("Geometry root must not be null");
    if (vertices.size() < 2)
        throw std::invalid_argument("At least two vertices are required");
    if (!std::isfinite(target_position.X()) || !std::isfinite(target_position.Y())
        || !std::isfinite(target_position.Z()))
        throw std::invalid_argument("Merge target position must be finite");

    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> root_vertices;
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> selected_vertices;
    TopExp::MapShapes(root, TopAbs_VERTEX, root_vertices);
    double destination_tolerance = Precision::Confusion();
    for (const TopoDS_Vertex& vertex : vertices) {
        if (vertex.IsNull() || !root_vertices.Contains(vertex))
            throw std::invalid_argument("Selected vertex does not belong to the geometry root");
        if (selected_vertices.Contains(vertex))
            throw std::invalid_argument("Selected vertices must not contain duplicates");
        selected_vertices.Add(vertex);
        destination_tolerance = std::max({ destination_tolerance,
            BRep_Tool::Tolerance(vertex), BRep_Tool::Pnt(vertex).Distance(target_position) });
    }

    try {
        TopoDS_Vertex destination = BRepBuilderAPI_MakeVertex(target_position);
        BRep_Builder vertex_builder;
        vertex_builder.UpdateVertex(destination, destination_tolerance);

        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> root_edges;
        TopExp::MapShapes(root, TopAbs_EDGE, root_edges);
        int removed_edges = 0;
        occ::handle<BRepTools_ReShape> reshaper = new BRepTools_ReShape();
        ShapeBuild_Edge edge_builder;
        for (int edge_index = 1; edge_index <= root_edges.Extent(); ++edge_index) {
            const TopoDS_Edge current_edge = TopoDS::Edge(root_edges.FindKey(edge_index));
            TopoDS_Vertex start;
            TopoDS_Vertex end;
            TopExp::Vertices(current_edge, start, end, true);
            if (!start.IsNull() && !end.IsNull()
                && selected_vertices.Contains(start) && selected_vertices.Contains(end)) {
                reshaper->Remove(current_edge);
                ++removed_edges;
                continue;
            }
            const bool replace_start = !start.IsNull() && selected_vertices.Contains(start);
            const bool replace_end = !end.IsNull() && selected_vertices.Contains(end);
            if (!replace_start && !replace_end)
                continue;
            const TopoDS_Edge rebuilt_edge = edge_builder.CopyReplaceVertices(
                current_edge,
                replace_start ? destination : start,
                replace_end ? destination : end);
            if (rebuilt_edge.IsNull())
                throw std::runtime_error("OpenCASCADE failed to rebuild an adjacent edge");
            reshaper->Replace(current_edge, rebuilt_edge);
        }

        TopoDS_Shape raw_result = reshaper->Apply(root);
        if (raw_result.ShapeType() == TopAbs_COMPOUND) {
            BRep_Builder builder;
            TopoDS_Compound compound;
            builder.MakeCompound(compound);
            bool removed_top_level_vertex = false;
            for (TopoDS_Iterator it(raw_result); it.More(); it.Next()) {
                const TopoDS_Shape& child = it.Value();
                if (child.ShapeType() == TopAbs_VERTEX && selected_vertices.Contains(child)) {
                    removed_top_level_vertex = true;
                    continue;
                }
                builder.Add(compound, child);
            }
            if (removed_top_level_vertex)
                builder.Add(compound, destination);
            raw_result = compound;
        }

        TopoDS_Shape result = fixAndValidate(raw_result, "Merging the vertices");
        if (countSubshapes(result, TopAbs_VERTEX)
                != root_vertices.Extent() - static_cast<int>(vertices.size()) + 1
            || countSubshapes(result, TopAbs_EDGE) != root_edges.Extent() - removed_edges)
            throw std::runtime_error("Merging the vertices did not produce the expected topology");
        return result;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        throw std::runtime_error(detail
                ? std::string("OpenCASCADE failed to merge the vertices: ") + detail
                : "OpenCASCADE failed to merge the vertices");
    }
}

TopoDS_Shape GeometryTopologyEditor::mergeFaces(
    const TopoDS_Shape& root,
    const std::vector<TopoDS_Face>& faces)
{
    if (root.IsNull())
        throw std::invalid_argument("Geometry root must not be null");
    if (faces.size() < 2)
        throw std::invalid_argument("At least two faces are required");

    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> root_faces;
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> selected_faces;
    TopExp::MapShapes(root, TopAbs_FACE, root_faces);
    for (const TopoDS_Face& face : faces) {
        if (face.IsNull())
            throw std::invalid_argument("Selected face must not be null");
        if (!root_faces.Contains(face))
            throw std::invalid_argument("Selected face does not belong to the geometry root");
        if (selected_faces.Contains(face))
            throw std::invalid_argument("Selected faces must not contain duplicates");
        selected_faces.Add(face);
    }

    try {
        using ShapeAncestors = NCollection_IndexedDataMap<TopoDS_Shape,
            NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher>;
        ShapeAncestors edge_faces;
        TopExp::MapShapesAndUniqueAncestors(
            root, TopAbs_EDGE, TopAbs_FACE, edge_faces);

        ShapeUpgrade_UnifySameDomain unifier(root, false, true, false);
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> shared_edges;
        // 只有两侧全部属于选择集的公共边允许消失，其余边界一律保留。
        for (int edge_index = 1; edge_index <= edge_faces.Extent(); ++edge_index) {
            const NCollection_List<TopoDS_Shape>& ancestors = edge_faces.FindFromIndex(edge_index);
            bool is_selected_boundary = ancestors.Extent() >= 2;
            for (NCollection_List<TopoDS_Shape>::Iterator it(ancestors); it.More(); it.Next()) {
                if (!selected_faces.Contains(it.Value())) {
                    is_selected_boundary = false;
                    break;
                }
            }
            if (is_selected_boundary)
                shared_edges.Add(edge_faces.FindKey(edge_index));
            else
                unifier.KeepShape(edge_faces.FindKey(edge_index));
        }
        unifier.Build();

        const TopoDS_Shape result = unifier.Shape();
        if (result.IsNull())
            throw std::runtime_error("OpenCASCADE returned an empty face merge result");

        // 原子操作的契约是全部选中面合成一个面，部分成功也视为失败。
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> result_faces;
        TopExp::MapShapes(result, TopAbs_FACE, result_faces);
        const int expected_face_count = root_faces.Extent()
            - static_cast<int>(faces.size()) + 1;
        if (result_faces.Extent() != expected_face_count)
            throw std::runtime_error("Selected faces are disconnected or do not share the same domain");

        // 先确认共享边已经从结果 Face 的拓扑边界中消失。
        if (shared_edges.IsEmpty())
            throw std::runtime_error("Selected faces do not have a shared edge");
        for (int face_index = 1; face_index <= result_faces.Extent(); ++face_index) {
            const TopoDS_Shape& result_face = result_faces.FindKey(face_index);
            if (!result_face.IsNull()) {
                NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> face_edges;
                TopExp::MapShapes(result_face, TopAbs_EDGE, face_edges);
                for (int edge_index = 1; edge_index <= shared_edges.Extent(); ++edge_index) {
                    if (face_edges.Contains(shared_edges.FindKey(edge_index)))
                        throw std::runtime_error("Merging the faces retained an internal shared edge");
                }
            }
        }

        TopoDS_Shape cleaned_result = result;
        if (result.ShapeType() == TopAbs_COMPOUND) {
            BRep_Builder builder;
            TopoDS_Compound compound;
            builder.MakeCompound(compound);
            bool removed_top_level_edge = false;
            for (TopoDS_Iterator it(result); it.More(); it.Next()) {
                const TopoDS_Shape& child = it.Value();
                bool is_shared_edge = child.ShapeType() == TopAbs_EDGE
                    && shared_edges.Contains(child);
                // BRepFeat 可能为面内分割边创建新的 TShape，需按几何重合识别原顶层工具边。
                if (child.ShapeType() == TopAbs_EDGE && !is_shared_edge) {
                    const TopoDS_Edge child_edge = TopoDS::Edge(child);
                    for (int edge_index = 1; edge_index <= shared_edges.Extent(); ++edge_index) {
                        const TopoDS_Edge shared_edge =
                            TopoDS::Edge(shared_edges.FindKey(edge_index));
                        if (BRepTools::Compare(child_edge, shared_edge)) {
                            is_shared_edge = true;
                            break;
                        }
                    }
                }
                if (is_shared_edge) {
                    removed_top_level_edge = true;
                    continue;
                }
                builder.Add(compound, child);
            }
            if (removed_top_level_edge)
                cleaned_result = compound;
        }

        // 创建出来的分割边可能仍是根 Compound 的独立子节点，合并面时一并消费。
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> cleaned_edges;
        TopExp::MapShapes(cleaned_result, TopAbs_EDGE, cleaned_edges);
        const int expected_edge_count = countSubshapes(root, TopAbs_EDGE) - shared_edges.Extent();
        if (cleaned_edges.Extent() != expected_edge_count)
            throw std::runtime_error("Merging the faces did not remove their shared edges");
        for (int edge_index = 1; edge_index <= shared_edges.Extent(); ++edge_index) {
            if (cleaned_edges.Contains(shared_edges.FindKey(edge_index)))
                throw std::runtime_error("Merging the faces retained an internal shared edge");
        }

        if (!BRepCheck_Analyzer(cleaned_result).IsValid())
            throw std::runtime_error("Merging the faces produced invalid topology");
        return cleaned_result;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        throw std::runtime_error(detail
                ? std::string("OpenCASCADE failed to merge the faces: ") + detail
                : "OpenCASCADE failed to merge the faces");
    }
}

TopoDS_Shape GeometryTopologyEditor::mergeEdges(
    const TopoDS_Shape& root,
    const std::vector<TopoDS_Edge>& edges)
{
    if (root.IsNull())
        throw std::invalid_argument("Geometry root must not be null");
    if (edges.size() < 2)
        throw std::invalid_argument("At least two edges are required");

    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> root_edges;
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> selected_edges;
    TopExp::MapShapes(root, TopAbs_EDGE, root_edges);
    for (const TopoDS_Edge& edge : edges) {
        if (edge.IsNull())
            throw std::invalid_argument("Selected edge must not be null");
        if (!root_edges.Contains(edge))
            throw std::invalid_argument("Selected edge does not belong to the geometry root");
        if (selected_edges.Contains(edge))
            throw std::invalid_argument("Selected edges must not contain duplicates");
        selected_edges.Add(edge);
    }

    try {
        using ShapeAncestors = NCollection_IndexedDataMap<TopoDS_Shape,
            NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher>;
        ShapeAncestors vertex_edges;
        TopExp::MapShapesAndUniqueAncestors(
            root, TopAbs_VERTEX, TopAbs_EDGE, vertex_edges);

        ShapeUpgrade_UnifySameDomain unifier(root, true, false, false);
        // 仅允许两条选中边独占的中间点消失，端点和分支点必须保留。
        for (int vertex_index = 1; vertex_index <= vertex_edges.Extent(); ++vertex_index) {
            const NCollection_List<TopoDS_Shape>& ancestors = vertex_edges.FindFromIndex(vertex_index);
            bool is_selected_joint = ancestors.Extent() == 2;
            for (NCollection_List<TopoDS_Shape>::Iterator it(ancestors); it.More(); it.Next()) {
                if (!selected_edges.Contains(it.Value())) {
                    is_selected_joint = false;
                    break;
                }
            }
            if (!is_selected_joint)
                unifier.KeepShape(vertex_edges.FindKey(vertex_index));
        }
        unifier.Build();

        const TopoDS_Shape result = unifier.Shape();
        if (result.IsNull())
            throw std::runtime_error("OpenCASCADE returned an empty edge merge result");

        // 原子操作的契约是全部选中边合成一条边，部分成功也视为失败。
        const int expected_edge_count = root_edges.Extent()
            - static_cast<int>(edges.size()) + 1;
        if (countSubshapes(result, TopAbs_EDGE) != expected_edge_count)
            throw std::runtime_error("Selected edges are disconnected or do not share the same domain");
        if (!BRepCheck_Analyzer(result).IsValid())
            throw std::runtime_error("Merging the edges produced invalid topology");
        return result;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        throw std::runtime_error(detail
                ? std::string("OpenCASCADE failed to merge the edges: ") + detail
                : "OpenCASCADE failed to merge the edges");
    }
}

TopoDS_Shape GeometryTopologyEditor::splitFace(
    const TopoDS_Shape& root,
    const TopoDS_Face& target_face,
    const std::vector<TopoDS_Edge>& splitting_edges)
{
    if (root.IsNull() || target_face.IsNull())
        throw std::invalid_argument("Geometry root and target face must not be null");
    if (splitting_edges.empty())
        throw std::invalid_argument("At least one splitting edge is required");

    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> root_faces;
    TopExp::MapShapes(root, TopAbs_FACE, root_faces);
    if (!root_faces.Contains(target_face))
        throw std::invalid_argument("Target face does not belong to the geometry root");

    try {
        for (const TopoDS_Edge& edge : splitting_edges) {
            if (edge.IsNull())
                throw std::invalid_argument("Splitting edge must not be null");
            if (!isEdgeOnFace(edge, target_face))
                throw std::invalid_argument("Splitting edge does not lie on the target face");
        }

        // BRepFeat 在完整 root 上替换目标 Face，保留其所在 Shell/Solid 及其他根子形状。
        BRepFeat_SplitShape splitter(root);
        for (const TopoDS_Edge& edge : splitting_edges)
            splitter.Add(edge, target_face);
        splitter.Build();

        if (!splitter.IsDone())
            throw std::runtime_error("OpenCASCADE failed to split the target face");

        // 没有生成至少两个替代面说明分割边未形成有效切分，禁止产生空效果写回。
        if (splitter.Modified(target_face).Extent() < 2)
            throw std::runtime_error("Splitting edges did not divide the target face");

        TopoDS_Shape result = splitter.Shape();
        if (result.IsNull())
            throw std::runtime_error("OpenCASCADE returned an empty split result");
        if (!BRepCheck_Analyzer(result).IsValid())
            throw std::runtime_error("Splitting the face produced invalid topology");
        return result;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        throw std::runtime_error(detail
                ? std::string("OpenCASCADE failed to split the face: ") + detail
                : "OpenCASCADE failed to split the face");
    }
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
