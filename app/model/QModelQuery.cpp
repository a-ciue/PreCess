#include "QModelQuery.h"
#include "MeshTopologyDiagnostics.h"
#include "ModelLayer.h"
#include "MeshData.h"
#include "GeometryData.h"
#include "GeometryDataVtk.h"
#include "ComponentData.h"

#include <QVariantList>
#include <QString>
#include <stdexcept>
#include <limits>
#include <TopoDS_Shape.hxx>
#include <spdlog/spdlog.h>

namespace {

// 将一个属性表追加为 QML 可直接使用的属性渲染条目。
void appendAttributeInfo(
    QVariantList& out,
    const std::map<std::string, std::vector<double>>& attributes,
    Element::Type type,
    const QString& type_name,
    int attr_type,
    size_t tuple_count)
{
    for (const auto& [name, values] : attributes) {
        const int component_count = tuple_count > 0 && values.size() % tuple_count == 0
            ? static_cast<int>(values.size() / tuple_count)
            : 0;

        // 前端只展示原始名称，实体类型和分量数由相邻字段单独展示。
        QString display_name = QString::fromStdString(name);
        if (display_name.startsWith("v_") || display_name.startsWith("e_")
            || display_name.startsWith("f_") || display_name.startsWith("s_"))
            display_name.remove(0, 2);
        if (component_count > 0) {
            const QString component_suffix = "_" + QString::number(component_count);
            if (display_name.endsWith(component_suffix))
                display_name.chop(component_suffix.size());
        }

        QVariantMap item;
        item["name"] = QString::fromStdString(name);
        item["displayName"] = display_name;
        item["type"] = static_cast<int>(type);
        item["typeName"] = type_name;
        item["attrType"] = attr_type;
        item["componentCount"] = component_count;
        out.append(item);
    }
}

}

QModelQuery::QModelQuery(ModelLayer* mgr, QObject* parent)
        : QObject(parent), m_manager(mgr) {
}

std::optional<MeshDataVtk> QModelQuery::getMeshData(Index model_id)
{
    using namespace std;
    auto ids = getComponentIds(model_id);
    ComponentData* comp = nullptr;
    for (Index cid : ids) {
        ComponentData* c = m_manager->findComponent(cid);
        if (c && c->mesh) {
            comp = c;
            break;
        }
    }
    if (!comp)
        return {};
    MeshData* md = comp->mesh.get();

    MeshDataVtk model_data {
        md->solid_types_, md->solid_vertices_, md->solid_vertices_offset_,
        md->solid_faces_vertices_, md->solid_faces_vertices_offset_,
        md->solid_faces_, md->solid_faces_offset_,
        md->face_vertices_, md->face_vertices_offset_,
        md->edge_vertices_,
        md->vertex_positions_,
        md->vertex_attributes_,md->edge_attributes_,md->face_attributes_,md->solid_attributes_,{},-1,
        std::make_shared<MeshTopologyDiagnosticResult>(MeshTopologyDiagnostics::analyze(comp->mesh_adjacency, *md)) };

    // 添加所有块
    auto block_datas = std::make_shared<BlockDatas>();
    for (const auto& [block_id, block] : md->blocks_) {
        BlockData block_data;
        block_data.id = block_id;

        // 添加该块中所有的patch faces
        vector<Index>& block_faces = block_data.faces_;
        for (const auto& patch_id : block->patchIDs) {
            vector<Index> patch_faces = md->patches_[patch_id]->faces;
            block_faces.insert(block_faces.end(), patch_faces.begin(), patch_faces.end());
        }

        block_datas->block_datas.push_back(block_data);
    }
    model_data.model_blocks_ = block_datas;

    return model_data;
}

std::optional<MeshDataVtk> QModelQuery::getMeshDataByComponent(Index component_id)
{
    ComponentData* comp = m_manager->findComponent(component_id);
    if (!comp || !comp->mesh) {
        return std::nullopt;
    }

    MeshData* md = comp->mesh.get();

    MeshDataVtk model_data {
        md->solid_types_, md->solid_vertices_, md->solid_vertices_offset_,
        md->solid_faces_vertices_, md->solid_faces_vertices_offset_,
        md->solid_faces_, md->solid_faces_offset_,
        md->face_vertices_, md->face_vertices_offset_,
        md->edge_vertices_,
        md->vertex_positions_,
        md->vertex_attributes_,
        md->edge_attributes_,
        md->face_attributes_,
        md->solid_attributes_,
        {},
        component_id,
        std::make_shared<MeshTopologyDiagnosticResult>(MeshTopologyDiagnostics::analyze(comp->mesh_adjacency, *md))
    };

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

    model_data.model_blocks_ = block_datas;
    return model_data;
}

std::optional<Index> QModelQuery::findEdgeByEndpoints(Index component_id, Index p0, Index p1)
{
    ComponentData* comp = m_manager->findComponent(component_id);
    if (!comp || !comp->mesh)
        return std::nullopt;

    auto& adjacency = comp->mesh_adjacency;
    auto edge = adjacency.findEdgeByEndpoints(*comp->mesh, p0, p1);
    if (!edge)
        return std::nullopt;
    // 句柄仅供当轮中转，对外统一给稳定局部边 id
    return adjacency.edgeStableId(*comp->mesh, *edge);
}

Index QModelQuery::pointGlobalId(Index component_id, Index local_point_id) const
{
    ComponentData* comp = m_manager->findComponent(component_id);
    if (!comp || local_point_id < 0
        || local_point_id >= static_cast<Index>(comp->point_global_ids_.size()))
        return -1;
    return comp->point_global_ids_[static_cast<size_t>(local_point_id)];
}

std::vector<GeometryDataVtk> QModelQuery::getGeometryVtkData(Index model_id)
{
    std::vector<GeometryDataVtk> result;
    for (Index cid : getComponentIds(model_id)) {
        ComponentData* comp = m_manager->findComponent(cid);
        if (!comp || !comp->geometry || !comp->geometry->rootShape)
            continue;

        result.push_back(GeometryDataVtk { *comp->geometry->rootShape, comp->id, &comp->geometry->index });
    }

    return result;
}

std::optional<GeometryDataVtk> QModelQuery::getGeometryVtkDataByComponent(Index component_id)
{
    ComponentData* comp = m_manager->findComponent(component_id);
    if (!comp || !comp->geometry || !comp->geometry->rootShape)
        return std::nullopt;

    return GeometryDataVtk { *comp->geometry->rootShape, comp->id, &comp->geometry->index };
}


std::vector<Index> QModelQuery::getComponentIds(Index model_id) const
{
    auto* model = m_manager->modelById(model_id);
    return model ? model->componentIds() : std::vector<Index>{};
}

int QModelQuery::findModelIdByComponent(Index component_id) const
{
    auto it = m_manager->component_to_model_.find(component_id);
    return it != m_manager->component_to_model_.end() ? it->second : -1;
}

bool QModelQuery::hasModel(Index model_id) const
{
    return m_manager->modelById(model_id) != nullptr;
}

bool QModelQuery::hasComponent(Index component_id) const
{
    return m_manager->findComponent(component_id) != nullptr;
}

QVariantList QModelQuery::getGeometryEdgeMappedPointIds(Index component_id, int localGeometryEdgeId)
{
    QVariantList out;

    ComponentData* comp = m_manager->findComponent(component_id);
    if (!comp || !comp->geometry)
        return out;

    auto gidOpt = resolveGeometryEdgeLocalId(component_id, localGeometryEdgeId);
    if (!gidOpt)
        return out;

    if (!comp->mapping)
        return out;

    auto it = comp->mapping->geometry_edge_to_mesh_point_ids.find(*gidOpt);
    if (it == comp->mapping->geometry_edge_to_mesh_point_ids.end())
        return out;

    for (Index pid : it->second)
        out.push_back(pid);
    return out;
}

QString QModelQuery::getModelName(Index model_id) const
{
    ModelData* model = m_manager->modelById(model_id);
    if (!model) {
        spdlog::error("模型不存在，无法获取名称,id:{}", model_id);
        return QString();
    }
    return QString::fromLocal8Bit(model->model_name_);
}

QString QModelQuery::getComponentName(Index component_id) const
{
    ComponentData* component = m_manager->findComponent(component_id);
    if (!component) {
        return QString();
    }
    return QString::fromLocal8Bit(component->name);
}

Q_INVOKABLE QStringList QModelQuery::getModelAttriName(Index model_id) const
{
    QStringList attri_list;
    ModelData* model = m_manager->modelById(model_id);
    if (!model) {
        spdlog::error("模型不存在，无法获取属性名，id:{}", model_id);
        return {};
    }
    auto ids = getComponentIds(model_id);
    ComponentData* comp = nullptr;
    for (Index cid : ids) {
        ComponentData* c = m_manager->findComponent(cid);
        if (c && c->mesh) {
            comp = c;
            break;
        }
    }
    if (!comp)
        return {};
    MeshData* mesh = comp->asMeshData();
    if (mesh) {
        for (const auto& [name, data] : mesh->vertex_attributes_) {
            attri_list.append(QString::fromStdString(name));
        }
        for (const auto& [name, data] : mesh->edge_attributes_) {
            attri_list.append(QString::fromStdString(name));
        }
        for (const auto& [name, data] : mesh->face_attributes_) {
            attri_list.append(QString::fromStdString(name));
        }
        for (const auto& [name, data] : mesh->solid_attributes_) {
            attri_list.append(QString::fromStdString(name));
        }
        spdlog::info("attri_list.size(): {}", attri_list.size());
    }
    return attri_list;
}

Q_INVOKABLE QList<Element::Type> QModelQuery::getModelAttriType(Index model_id) const
{
    QList<Element::Type> type_list;
    ModelData* model = m_manager->modelById(model_id);
    if (!model) {
        spdlog::error( "模型不存在，无法获取属性类型，id:{}",model_id);
        return {};
    }
    auto ids = getComponentIds(model_id);
    ComponentData* comp = nullptr;
    for (Index cid : ids) {
        ComponentData* c = m_manager->findComponent(cid);
        if (c && c->mesh) {
            comp = c;
            break;
        }
    }
    if (!comp)
        return {};
    MeshData* mesh = comp->asMeshData();
    if (mesh) {
        for (const auto& [name, data] : mesh->vertex_attributes_) {
            type_list.append(Element::Type::Vertex);
        }
        for (const auto& [name, data] : mesh->edge_attributes_) {
            type_list.append(Element::Type::Edge);
        }
        for (const auto& [name, data] : mesh->face_attributes_) {
            type_list.append(Element::Type::Face);
        }
        for (const auto& [name, data] : mesh->solid_attributes_) {
            type_list.append(Element::Type::Solid);
        }
        spdlog::info("type_list.size(): {}", type_list.size());
    }
    return type_list;
}

QVariantList QModelQuery::getComponentAttriInfo(Index component_id) const
{
    QVariantList out;
    ComponentData* comp = m_manager->findComponent(component_id);
    if (!comp || !comp->mesh) {
        return out;
    }

    const MeshData& mesh = *comp->mesh;
    const size_t vertex_count = static_cast<size_t>(mesh.vertex_count_);
    const size_t edge_count = mesh.edge_vertices_.size() / 2;
    const size_t face_count = mesh.face_vertices_offset_.empty()
        ? 0
        : mesh.face_vertices_offset_.size() - 1;
    const size_t solid_count = mesh.solid_vertices_offset_.empty()
        ? 0
        : mesh.solid_vertices_offset_.size() - 1;

    appendAttributeInfo(out, mesh.vertex_attributes_, Element::Type::Vertex, "点", 0, vertex_count);
    appendAttributeInfo(out, mesh.edge_attributes_, Element::Type::Edge, "边", 1, edge_count);
    appendAttributeInfo(out, mesh.face_attributes_, Element::Type::Face, "面", 2, face_count);
    appendAttributeInfo(out, mesh.solid_attributes_, Element::Type::Solid, "体", 3, solid_count);
    return out;
}

QVariantList QModelQuery::listModels() const
{
    QVariantList out;
    if (!m_manager)
        return out;

    for (const auto& [mid, modelPtr] : m_manager->models_) {
        QVariantMap m;
        m["model_id"] = mid;
        m["name"] = modelPtr ? QString::fromLocal8Bit(modelPtr->model_name_) : QString();

        int compCount = 0;

        const std::vector<Index> cids = getComponentIds(mid);
        compCount = (int)cids.size();

        m["component_count"] = compCount;

        out.push_back(m);
    }
    return out;
}

QVariantList QModelQuery::getComponentsSummary(Index model_id) const
{
    QVariantList out;
    if (!m_manager)
        return out;

    const std::vector<Index> cids = getComponentIds(model_id);
    out.reserve((int)cids.size());

    for (Index cid : cids) {
        ComponentData* c = m_manager->findComponent(cid);
        if (!c)
            continue;

        QVariantMap m;
        m["component_id"] = cid;
        m["name"] = QString::fromLocal8Bit(c->name);
        m["has_mesh"] = (bool)c->mesh;
        m["has_geometry"] = (bool)c->geometry;
        m["material_id"] = c->material_id;

        out.push_back(m);
    }
    return out;
}

QVariantMap QModelQuery::getMeshSummary(Index component_id) const
{
    QVariantMap m;
    if (!m_manager)
        return m;

    ComponentData* c = m_manager->findComponent(component_id);
    if (!c || !c->mesh) {
        m["has_mesh"] = false;
        return m;
    }

    const MeshData& md = *c->mesh;
    m["has_mesh"] = true;

    m["vertex_count"] = md.vertex_count_;
    m["edge_count"] = (int)(md.edge_vertices_.size() / 2);

    int faceCount = 0;
    if (md.face_vertices_offset_.size() >= 1)
        faceCount = (int)md.face_vertices_offset_.size() - 1;
    m["face_count"] = faceCount;

    m["solid_count"] = (int)md.solid_types_.size();

    return m;
}

QVariantMap QModelQuery::getGeometrySummary(Index component_id) const
{
    QVariantMap m;
    if (!m_manager)
        return m;

    ComponentData* c = m_manager->findComponent(component_id);
    if (!c || !c->geometry || !c->geometry->rootShape) {
        m["has_geometry"] = false;
        return m;
    }

    c->geometry->ensureIndexBuilt(m_manager->geomRegistry());

    const GeometrySubshapeIndex& idx = c->geometry->index;
    m["has_geometry"] = true;

    auto countOf = [&](TopAbs_ShapeEnum t) -> int {
        const int ti = GeometrySubshapeIndex::typeIndex(t);
        if (ti < 0)
            return 0;
        return idx.type_maps[(size_t)ti].Extent();
    };

    m["vertex_count"] = countOf(TopAbs_VERTEX);
    m["edge_count"] = countOf(TopAbs_EDGE);
    m["face_count"] = countOf(TopAbs_FACE);
    m["solid_count"] = countOf(TopAbs_SOLID);

    return m;
}

std::optional<GeomFaceId> QModelQuery::resolveGeometryFaceLocalId(Index component_id, int localFaceId)
{
    ComponentData* comp = m_manager->findComponent(component_id);
    if (!comp || !comp->geometry || !comp->geometry->rootShape)
        return std::nullopt;

    comp->geometry->ensureIndexBuilt(m_manager->geomRegistry());

    GeomFaceId gid = comp->geometry->index.faceGlobalId(localFaceId);
    if (gid == kInvalidGeomFaceId)
        return std::nullopt;

    return gid;
}

std::optional<GeomEdgeId> QModelQuery::resolveGeometryEdgeLocalId(Index component_id, int localEdgeId)
{
    ComponentData* comp = m_manager->findComponent(component_id);
    if (!comp || !comp->geometry || !comp->geometry->rootShape)
        return std::nullopt;

    comp->geometry->ensureIndexBuilt(m_manager->geomRegistry());

    GeomEdgeId gid = comp->geometry->index.edgeGlobalId(localEdgeId);
    if (gid == kInvalidGeomEdgeId)
        return std::nullopt;

    return gid;
}

std::optional<GeomVertexId> QModelQuery::resolveGeometryVertexLocalId(Index component_id, int localVertexId)
{
    ComponentData* comp = m_manager->findComponent(component_id);
    if (!comp || !comp->geometry || !comp->geometry->rootShape)
        return std::nullopt;

    comp->geometry->ensureIndexBuilt(m_manager->geomRegistry());

    GeomVertexId gid = comp->geometry->index.vertexGlobalId(localVertexId);
    if (gid == kInvalidGeomVertexId)
        return std::nullopt;

    return gid;
}

std::optional<GeomSolidId> QModelQuery::resolveGeometrySolidLocalId(Index component_id, int localSolidId)
{
    ComponentData* comp = m_manager->findComponent(component_id);
    if (!comp || !comp->geometry || !comp->geometry->rootShape)
        return std::nullopt;

    comp->geometry->ensureIndexBuilt(m_manager->geomRegistry());

    GeomSolidId gid = comp->geometry->index.solidGlobalId(localSolidId);
    if (gid == kInvalidGeomSolidId)
        return std::nullopt;

    return gid;
}
