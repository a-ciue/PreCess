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
#include <Geom_Curve.hxx>
#include <Geom_Surface.hxx>
#include <GeomAbs_SurfaceType.hxx>
#include <GProp_GProps.hxx>
#include <Precision.hxx>
#include <ShapeUpgrade_UnifySameDomain.hxx>
#include <ShapeFix_Shape.hxx>
#include <ShapeBuild_Edge.hxx>
#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp.hxx>
#include <NCollection_IndexedMap.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopTools_ShapeMapHasher.hxx>
#include <gp_Pnt.hxx>
#include <gp_Pnt2d.hxx>
#include <GCPnts_AbscissaPoint.hxx>
#include <TopExp_Explorer.hxx>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
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
 * @brief 保存重复面和相交面筛选需要的低成本几何量。
 */
struct FaceMetrics {
    TopoDS_Face face;
    Bnd_Box box;
    gp_Pnt center;
    double area { 0.0 };
    double perimeter { 0.0 };
    GeomAbs_SurfaceType surface_type { GeomAbs_OtherSurface };
};

FaceMetrics measureFace(const TopoDS_Face& face)
{
    FaceMetrics metrics;
    metrics.face = face;
    BRepBndLib::AddOptimal(face, metrics.box, false);

    GProp_GProps surface_properties;
    BRepGProp::SurfaceProperties(face, surface_properties);
    metrics.area = std::abs(surface_properties.Mass());
    metrics.center = surface_properties.CentreOfMass();

    GProp_GProps linear_properties;
    BRepGProp::LinearProperties(face, linear_properties);
    metrics.perimeter = std::abs(linear_properties.Mass());
    metrics.surface_type = BRepAdaptor_Surface(face).GetType();
    return metrics;
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
 * @brief 判断两个独立 Face 是否在清理容差内覆盖相同区域。
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

bool shareTopologicalEdge(const TopoDS_Face& first, const TopoDS_Face& second)
{
    NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> first_edges;
    TopExp::MapShapes(first, TopAbs_EDGE, first_edges);
    for (TopExp_Explorer edge(second, TopAbs_EDGE); edge.More(); edge.Next()) {
        if (first_edges.Contains(edge.Current()))
            return true;
    }
    return false;
}

bool facesIntersect(const FaceMetrics& first, const FaceMetrics& second, double tolerance)
{
    Bnd_Box first_box = first.box;
    Bnd_Box second_box = second.box;
    first_box.Enlarge(tolerance);
    second_box.Enlarge(tolerance);
    if (first_box.IsOut(second_box) || shareTopologicalEdge(first.face, second.face))
        return false;

    if (commonArea(first.face, second.face, tolerance) > tolerance * tolerance)
        return true;

    try {
        BRepAlgoAPI_Section section(first.face, second.face, false);
        section.SetFuzzyValue(tolerance);
        section.Build();
        if (!section.IsDone())
            return false;
        for (TopExp_Explorer edge(section.Shape(), TopAbs_EDGE); edge.More(); edge.Next())
            return true;
    } catch (const Standard_Failure&) {
        return false;
    }
    return false;
}
}

GeometryTopologyDiagnosticResult GeometryTopologyEditor::diagnoseTopology(
    const TopoDS_Shape& root,
    double cleanup_tolerance,
    const GeometryTopologyDiagnosticOptions& options)
{
    if (root.IsNull())
        throw std::invalid_argument("Geometry root must not be null");
    if (!std::isfinite(cleanup_tolerance) || cleanup_tolerance <= 0.0)
        throw std::invalid_argument("Cleanup tolerance must be greater than zero");

    GeometryTopologyDiagnosticResult result;

    if (options.edge_topology) {
        // 按相邻 Face 数量分类全部 Edge，规则与网格拓扑诊断保持一致。
        NCollection_IndexedDataMap<TopoDS_Shape,
            NCollection_List<TopoDS_Shape>, TopTools_ShapeMapHasher>
            edge_faces;
        TopExp::MapShapesAndAncestors(root, TopAbs_EDGE, TopAbs_FACE, edge_faces);
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> edges;
        TopExp::MapShapes(root, TopAbs_EDGE, edges);
        for (int edge_index = 1; edge_index <= edges.Extent(); ++edge_index) {
            const TopoDS_Edge edge = TopoDS::Edge(edges.FindKey(edge_index));
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
    if (needs_face_pairs || options.degenerated_faces) {
        NCollection_IndexedMap<TopoDS_Shape, TopTools_ShapeMapHasher> face_map;
        TopExp::MapShapes(root, TopAbs_FACE, face_map);
        faces.reserve(static_cast<size_t>(face_map.Extent()));
        const double minimum_area = cleanup_tolerance * cleanup_tolerance;
        for (int face_index = 1; face_index <= face_map.Extent(); ++face_index) {
            const TopoDS_Face face = TopoDS::Face(face_map.FindKey(face_index));
            try {
                FaceMetrics metrics = measureFace(face);
                if (options.degenerated_faces
                    && (metrics.area <= minimum_area
                        || BRepTools::OuterWire(face).IsNull()
                        || !BRepCheck_Analyzer(face).IsValid())) {
                    result.degenerated_faces.push_back(face);
                }
                faces.push_back(std::move(metrics));
            } catch (const Standard_Failure&) {
                // 无法取得基本几何量的 Face 本身就是退化面，不再参与面配对计算。
                if (options.degenerated_faces)
                    result.degenerated_faces.push_back(face);
            }
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
        for (size_t first = 0; first < faces.size(); ++first) {
            for (size_t second = first + 1; second < faces.size(); ++second) {
                if (areDuplicateFaces(faces[first], faces[second], cleanup_tolerance)) {
                    duplicate_pairs.emplace_back(first, second);
                    merge_groups(first, second);
                }
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

    if (options.intersecting_faces) {
        // 重复面已经有独立类别，不再同时报告为相交面。
        for (size_t first = 0; first < faces.size(); ++first) {
            for (size_t second = first + 1; second < faces.size(); ++second) {
                if (find_parent(first) == find_parent(second)
                    || !facesIntersect(faces[first], faces[second], cleanup_tolerance)) {
                    continue;
                }
                result.intersecting_face_pairs.push_back(
                    { faces[first].face, faces[second].face });
            }
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
