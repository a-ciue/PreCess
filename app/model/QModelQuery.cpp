#include "QModelQuery.h"
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

    // 构造 ModelData
    MeshDataVtk model_data { 
        md->solid_types_, md->solid_vertices_, md->solid_vertices_offset_,
        md->solid_faces_vertices_, md->solid_faces_vertices_offset_,
        md->solid_faces_, md->solid_faces_offset_,
        md->face_vertices_, md->face_vertices_offset_,
        md->edge_vertices_,
        md->local_to_global_,
        md->vertex_attributes_,md->edge_attributes_,md->face_attributes_,md->solid_attributes_,{},-1 };

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
        md->local_to_global_,
        md->vertex_attributes_,
        md->edge_attributes_,
        md->face_attributes_,
        md->solid_attributes_,
        {},
        component_id
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

const std::vector<std::array<double, 3>>& QModelQuery::globalPoints() const
{
    return m_manager->globalPoints();
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

    auto it = comp->mapping->geometry_edge_to_mesh_point_gids.find(*gidOpt);
    if (it == comp->mapping->geometry_edge_to_mesh_point_gids.end())
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