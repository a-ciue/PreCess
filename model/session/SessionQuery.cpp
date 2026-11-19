/**
 * @file SessionQuery.cpp
 * @brief 会话层查询接口实现（自 QModelQuery 下沉）
 */
#include "SessionQuery.h"
#include "ComponentData.h"
#include "GeometryData.h"
#include "GeometrySubshapeIndex.h"
#include "MeshData.h"
#include "ModelLayer.h"

#include <TopAbs_ShapeEnum.hxx>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <numeric>
#include <utility>

namespace session {
namespace {

    /** @brief 保留原始两端值，内部选择三位有效数字的十进制等间隔刻度。 */
    std::vector<double> decimalHistogramEdges(double lower, double upper, size_t max_bins)
    {
        const double step_hint = (upper - lower) / max_bins;
        if (!std::isnormal(step_hint))
            return { };
        const int exponent = static_cast<int>(std::floor(std::log10(step_hint))) - 1;
        const double unit = std::pow(10.0, exponent);
        const double low_tick = lower / unit;
        const double high_tick = upper / unit;
        // 大基值上的窄分布改由调用方用相对偏移表示。
        if (std::max(std::abs(low_tick), std::abs(high_tick)) > 1e15)
            return { };

        // 只要求内部刻度简短，两端直接采用真实值，首尾箱可以较短。
        for (const int step : { 10, 20, 25, 50, 100 }) {
            std::vector<double> edges { lower };
            const double first = std::floor(low_tick / step);
            const double last = std::ceil(high_tick / step);
            for (double index = first; index <= last; ++index) {
                const auto tick = static_cast<std::int64_t>(index) * step;
                // 按十进制读取，避免浮点乘加产生冗长尾数。
                const double edge = std::strtod((std::to_string(tick) + "e" + std::to_string(exponent)).c_str(), nullptr);
                if (edge <= lower || edge >= upper)
                    continue;
                auto significant = tick;
                while (significant != 0 && significant % 10 == 0)
                    significant /= 10;
                if (std::abs(significant) >= 1000) {
                    edges.clear();
                    break;
                }
                edges.push_back(edge);
            }
            if (edges.empty())
                continue;
            edges.push_back(upper);
            if (edges.size() - 1 <= max_bins)
                return edges;
        }
        return { };
    }

    // 计算属性展示名：去除 v_/e_/f_/s_ 前缀与分量数后缀，前端只展示原始名以外的部分
    std::string attributeDisplayName(const std::string& name, int component_count)
    {
        std::string display = name;
        if (display.rfind("v_", 0) == 0 || display.rfind("e_", 0) == 0
            || display.rfind("f_", 0) == 0 || display.rfind("s_", 0) == 0)
            display.erase(0, 2);
        if (component_count > 0) {
            const std::string suffix = "_" + std::to_string(component_count);
            if (display.size() >= suffix.size()
                && display.compare(display.size() - suffix.size(), suffix.size(), suffix) == 0)
                display.resize(display.size() - suffix.size());
        }
        return display;
    }

    // 将一张属性表的全部条目追加为属性渲染条目（实体类型与分量数由相邻字段单独携带）
    void appendAttributeInfos(
        std::vector<AttributeInfo>& out,
        const std::map<std::string, std::vector<double>>& attributes,
        ElementEnum::Type type,
        const char* type_name,
        int attr_type,
        size_t tuple_count)
    {
        for (const auto& [name, values] : attributes) {
            const int component_count = tuple_count > 0 && values.size() % tuple_count == 0
                ? static_cast<int>(values.size() / tuple_count)
                : 0;
            out.push_back(AttributeInfo {
                name,
                attributeDisplayName(name, component_count),
                type,
                type_name,
                attr_type,
                component_count });
        }
    }

    // 由组件网格构建渲染视图（blocks_ 展平为块面表）；component_id 由调用方给定
    std::optional<MeshDataVtk> makeMeshView(ComponentData& comp, Index component_id)
    {
        if (!comp.mesh)
            return std::nullopt;
        MeshData* md = comp.mesh.get();

        MeshDataVtk view {
            md->solid_types_, md->solid_vertices_, md->solid_vertices_offset_,
            md->solid_faces_vertices_, md->solid_faces_vertices_offset_,
            md->solid_faces_, md->solid_faces_offset_,
            md->face_vertices_, md->face_vertices_offset_,
            md->edge_vertices_,
            md->vertex_positions_,
            md->vertex_attributes_, md->edge_attributes_, md->face_attributes_, md->solid_attributes_,
            { }, component_id
        };

        // 添加所有块：块面表由块内各 patch 的面拼合
        auto block_datas = std::make_shared<BlockDatas>();
        for (const auto& [block_id, block] : md->blocks_) {
            BlockData block_data;
            block_data.id = block_id;

            std::vector<Index>& block_faces = block_data.faces_;
            for (const auto& patch_id : block->patchIDs) {
                std::vector<Index> patch_faces = md->patches_[patch_id]->faces;
                block_faces.insert(block_faces.end(), patch_faces.begin(), patch_faces.end());
            }

            block_datas->block_datas.push_back(block_data);
        }
        view.model_blocks_ = block_datas;
        return view;
    }

    // 取模型内首个带网格的组件；无网格组件返回 nullptr
    ComponentData* firstMeshComponent(ModelLayer& model, Index model_id)
    {
        for (Index cid : [&] {
                 auto* m = model.modelById(model_id);
                 return m ? m->componentIds() : std::vector<Index> { };
             }()) {
            ComponentData* c = model.findComponent(cid);
            if (c && c->mesh)
                return c;
        }
        return nullptr;
    }

}

SessionQuery::SessionQuery(ModelLayer& model)
    : model_(model)
{
}

std::optional<Index> SessionQuery::firstMeshComponentId(Index model_id) const
{
    ComponentData* comp = firstMeshComponent(model_, model_id);
    if (!comp)
        return std::nullopt;
    return comp->id;
}

std::optional<MeshDataVtk> SessionQuery::meshDataByComponent(Index component_id) const
{
    ComponentData* comp = model_.findComponent(component_id);
    if (!comp)
        return std::nullopt;
    return makeMeshView(*comp, component_id);
}

std::vector<GeometryDataVtk> SessionQuery::geometryDataByModel(Index model_id) const
{
    std::vector<GeometryDataVtk> result;
    for (Index cid : componentIds(model_id)) {
        ComponentData* comp = model_.findComponent(cid);
        if (!comp || !comp->geometry || !comp->geometry->rootShape)
            continue;
        result.push_back(GeometryDataVtk { *comp->geometry->rootShape, comp->id, &comp->geometry->index });
    }
    return result;
}

std::optional<GeometryDataVtk> SessionQuery::geometryDataByComponent(Index component_id) const
{
    ComponentData* comp = model_.findComponent(component_id);
    if (!comp || !comp->geometry || !comp->geometry->rootShape)
        return std::nullopt;
    return GeometryDataVtk { *comp->geometry->rootShape, comp->id, &comp->geometry->index };
}

std::vector<Index> SessionQuery::componentIds(Index model_id) const
{
    auto* model = model_.modelById(model_id);
    return model ? model->componentIds() : std::vector<Index> { };
}

Index SessionQuery::findModelIdByComponent(Index component_id) const
{
    auto it = model_.component_to_model_.find(component_id);
    return it != model_.component_to_model_.end() ? it->second : -1;
}

bool SessionQuery::hasModel(Index model_id) const
{
    return model_.modelById(model_id) != nullptr;
}

bool SessionQuery::hasComponent(Index component_id) const
{
    return model_.findComponent(component_id) != nullptr;
}

std::optional<std::string> SessionQuery::modelName(Index model_id) const
{
    ModelData* model = model_.modelById(model_id);
    if (!model)
        return std::nullopt;
    return model->model_name_;
}

std::optional<std::string> SessionQuery::componentName(Index component_id) const
{
    ComponentData* component = model_.findComponent(component_id);
    if (!component)
        return std::nullopt;
    return component->name;
}

Index SessionQuery::pointGlobalId(Index component_id, Index local_point_id) const
{
    ComponentData* comp = model_.findComponent(component_id);
    if (!comp || local_point_id < 0
        || local_point_id >= static_cast<Index>(comp->point_global_ids_.size()))
        return -1;
    return comp->point_global_ids_[static_cast<size_t>(local_point_id)];
}

std::optional<Index> SessionQuery::findEdgeByEndpoints(Index component_id, Index p0, Index p1) const
{
    ComponentData* comp = model_.findComponent(component_id);
    if (!comp || !comp->mesh)
        return std::nullopt;

    auto& adjacency = comp->mesh_adjacency;
    auto edge = adjacency.findEdgeByEndpoints(*comp->mesh, p0, p1);
    if (!edge)
        return std::nullopt;
    // 句柄仅供当轮中转，对外统一给稳定局部边 id
    return adjacency.edgeStableId(*comp->mesh, *edge);
}

std::vector<Index> SessionQuery::geometryEdgeMappedPointIds(Index component_id, int local_geometry_edge_id) const
{
    std::vector<Index> out;

    ComponentData* comp = model_.findComponent(component_id);
    if (!comp || !comp->geometry || !comp->mapping)
        return out;

    auto gid = resolveGeometryEdgeLocalId(component_id, local_geometry_edge_id);
    if (!gid)
        return out;

    auto it = comp->mapping->geometry_edge_to_mesh_point_ids.find(*gid);
    if (it == comp->mapping->geometry_edge_to_mesh_point_ids.end())
        return out;

    return it->second;
}

std::vector<ModelSummary> SessionQuery::listModels() const
{
    std::vector<ModelSummary> out;
    for (const auto& [model_id, model] : model_.models_) {
        ModelSummary summary;
        summary.model_id = model_id;
        summary.name = model ? model->model_name_ : std::string { };
        summary.component_count = static_cast<int>(componentIds(model_id).size());
        out.push_back(std::move(summary));
    }
    return out;
}

std::vector<ComponentSummary> SessionQuery::componentSummaries(Index model_id) const
{
    std::vector<ComponentSummary> out;
    for (Index cid : componentIds(model_id)) {
        ComponentData* comp = model_.findComponent(cid);
        if (!comp)
            continue;

        ComponentSummary summary;
        summary.component_id = cid;
        summary.name = comp->name;
        summary.has_mesh = static_cast<bool>(comp->mesh);
        summary.has_geometry = static_cast<bool>(comp->geometry);
        summary.material_id = comp->material_id;
        out.push_back(std::move(summary));
    }
    return out;
}

MeshSummary SessionQuery::meshSummary(Index component_id) const
{
    MeshSummary summary;
    ComponentData* comp = model_.findComponent(component_id);
    if (!comp || !comp->mesh)
        return summary;

    const MeshData& md = *comp->mesh;
    summary.has_mesh = true;
    summary.vertex_count = md.vertex_count_;
    summary.edge_count = static_cast<Index>(md.edge_vertices_.size() / 2);
    summary.face_count = md.face_vertices_offset_.empty() ? 0 : static_cast<Index>(md.face_vertices_offset_.size() - 1);
    summary.solid_count = static_cast<Index>(md.solid_types_.size());
    return summary;
}

GeometrySummary SessionQuery::geometrySummary(Index component_id) const
{
    GeometrySummary summary;
    ComponentData* comp = model_.findComponent(component_id);
    if (!comp || !comp->geometry || !comp->geometry->rootShape)
        return summary;

    comp->geometry->ensureIndexBuilt(model_.geomRegistry());

    const GeometrySubshapeIndex& idx = comp->geometry->index;
    summary.has_geometry = true;

    auto countOf = [&](TopAbs_ShapeEnum t) -> int {
        const int ti = GeometrySubshapeIndex::typeIndex(t);
        if (ti < 0)
            return 0;
        return idx.type_maps[static_cast<size_t>(ti)].Extent();
    };

    summary.vertex_count = countOf(TopAbs_VERTEX);
    summary.edge_count = countOf(TopAbs_EDGE);
    summary.face_count = countOf(TopAbs_FACE);
    summary.solid_count = countOf(TopAbs_SOLID);
    return summary;
}

std::vector<AttributeInfo> SessionQuery::componentAttributeInfos(Index component_id) const
{
    std::vector<AttributeInfo> out;
    ComponentData* comp = model_.findComponent(component_id);
    if (!comp || !comp->mesh)
        return out;

    const MeshData& mesh = *comp->mesh;
    const size_t vertex_count = static_cast<size_t>(mesh.vertex_count_);
    const size_t edge_count = mesh.edge_vertices_.size() / 2;
    const size_t face_count = mesh.face_vertices_offset_.empty()
        ? 0
        : mesh.face_vertices_offset_.size() - 1;
    const size_t solid_count = mesh.solid_vertices_offset_.empty()
        ? 0
        : mesh.solid_vertices_offset_.size() - 1;

    appendAttributeInfos(out, mesh.vertex_attributes_, ElementEnum::Type::Vertex, "点", 0, vertex_count);
    appendAttributeInfos(out, mesh.edge_attributes_, ElementEnum::Type::Edge, "边", 1, edge_count);
    appendAttributeInfos(out, mesh.face_attributes_, ElementEnum::Type::Face, "面", 2, face_count);
    appendAttributeInfos(out, mesh.solid_attributes_, ElementEnum::Type::Solid, "体", 3, solid_count);
    return out;
}

std::optional<ScalarHistogram> SessionQuery::scalarHistogram(Index component_id, const std::string& attribute) const
{
    const auto* component = model_.findComponent(component_id);
    if (!component || !component->mesh)
        return std::nullopt;
    const auto& mesh = *component->mesh;
    const std::vector<double>* values = nullptr;
    // 与渲染数组的构建和查找一致：跳过空、长度无效的数组，首个有效同名属性决定实体域。
    for (const auto& [attributes, tuple_count] : {
             std::pair { &mesh.vertex_attributes_, mesh.vertex_positions_.size() },
             std::pair { &mesh.edge_attributes_, mesh.edge_vertices_.size() / 2 },
             std::pair { &mesh.face_attributes_, mesh.face_vertices_offset_.empty() ? 0 : mesh.face_vertices_offset_.size() - 1 },
             std::pair { &mesh.solid_attributes_, mesh.solid_vertices_offset_.empty() ? 0 : mesh.solid_vertices_offset_.size() - 1 } }) {
        const auto found = attributes->find(attribute);
        if (found == attributes->end() || found->second.empty() || tuple_count == 0 || found->second.size() % tuple_count != 0)
            continue;
        // 有效多分量属性仍占据查找优先级，不改用后面的同名单值属性。
        if (found->second.size() != tuple_count)
            return std::nullopt;
        values = &found->second;
        break;
    }
    if (!values)
        return std::nullopt;

    double lower = std::numeric_limits<double>::infinity();
    double upper = -lower;
    // 完整原始属性不受人工隐藏、显示范围和裁剪影响。
    for (const double value : *values) {
        if (std::isfinite(value)) {
            lower = std::min(lower, value);
            upper = std::max(upper, value);
        }
    }
    if (lower > upper)
        return std::nullopt;
    constexpr size_t max_bins = 32;
    ScalarHistogram result;
    if (lower == upper) {
        result.edges = { lower, upper };
    } else {
        result.edges = decimalHistogramEdges(lower, upper, max_bins);
        const double span = upper - lower;
        if (result.edges.empty() && std::isfinite(span)) {
            result.edges = decimalHistogramEdges(0.0, span, max_bins);
            if (!result.edges.empty())
                result.origin = lower;
        }

        // 十进制刻度无法覆盖的极端值域保留可表示的边界。
        if (result.edges.empty()) {
            const double midpoint = std::midpoint(lower, upper);
            result.edges.resize(max_bins + 1);
            for (size_t i = 0; i <= max_bins; ++i) {
                const double fraction = static_cast<double>(i) / max_bins;
                result.edges[i] = fraction <= 0.5 ? std::lerp(lower, midpoint, fraction * 2)
                                                  : std::lerp(midpoint, upper, fraction * 2 - 1);
            }
            result.edges.erase(std::unique(result.edges.begin(), result.edges.end()), result.edges.end());
        }
    }
    result.counts.resize(result.edges.size() - 1);

    // 比较同一套内部边界，等于边界的样本进入右箱；最大值留在最后一箱。
    const auto first_boundary = result.edges.begin() + 1;
    const auto last_boundary = result.edges.end() - 1;
    for (double value : *values) {
        if (!std::isfinite(value))
            continue;
        if (result.origin)
            value -= *result.origin;
        const auto bin = std::upper_bound(first_boundary, last_boundary, value) - first_boundary;
        ++result.counts[bin];
    }
    return result;
}

std::optional<GeomFaceId> SessionQuery::resolveGeometryFaceLocalId(Index component_id, int local_face_id) const
{
    ComponentData* comp = model_.findComponent(component_id);
    if (!comp || !comp->geometry || !comp->geometry->rootShape)
        return std::nullopt;

    comp->geometry->ensureIndexBuilt(model_.geomRegistry());

    GeomFaceId gid = comp->geometry->index.faceGlobalId(local_face_id);
    if (gid == kInvalidGeomFaceId)
        return std::nullopt;
    return gid;
}

std::optional<GeomEdgeId> SessionQuery::resolveGeometryEdgeLocalId(Index component_id, int local_edge_id) const
{
    ComponentData* comp = model_.findComponent(component_id);
    if (!comp || !comp->geometry || !comp->geometry->rootShape)
        return std::nullopt;

    comp->geometry->ensureIndexBuilt(model_.geomRegistry());

    GeomEdgeId gid = comp->geometry->index.edgeGlobalId(local_edge_id);
    if (gid == kInvalidGeomEdgeId)
        return std::nullopt;
    return gid;
}

std::optional<GeomVertexId> SessionQuery::resolveGeometryVertexLocalId(Index component_id, int local_vertex_id) const
{
    ComponentData* comp = model_.findComponent(component_id);
    if (!comp || !comp->geometry || !comp->geometry->rootShape)
        return std::nullopt;

    comp->geometry->ensureIndexBuilt(model_.geomRegistry());

    GeomVertexId gid = comp->geometry->index.vertexGlobalId(local_vertex_id);
    if (gid == kInvalidGeomVertexId)
        return std::nullopt;
    return gid;
}

std::optional<GeomSolidId> SessionQuery::resolveGeometrySolidLocalId(Index component_id, int local_solid_id) const
{
    ComponentData* comp = model_.findComponent(component_id);
    if (!comp || !comp->geometry || !comp->geometry->rootShape)
        return std::nullopt;

    comp->geometry->ensureIndexBuilt(model_.geomRegistry());

    GeomSolidId gid = comp->geometry->index.solidGlobalId(local_solid_id);
    if (gid == kInvalidGeomSolidId)
        return std::nullopt;
    return gid;
}
}
