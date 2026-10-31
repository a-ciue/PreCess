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

#include <map>

namespace session {
namespace {

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
