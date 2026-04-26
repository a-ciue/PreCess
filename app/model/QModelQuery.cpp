#include "QModelQuery.h"
#include "ModelManager.h"
#include "MeshData.h"
#include "SplineData.h"
#include "SplineDataVtk.h"

#include <QVariantList>
#include <QString>
#include <stdexcept>
#include <limits>
#include <TopoDS_Shape.hxx>
#include <spdlog/spdlog.h>

QModelQuery::QModelQuery(ModelManager* mgr, QObject* parent)
        : QObject(parent), m_manager(mgr) {
}

std::optional<MeshDataVtk> QModelQuery::getMeshData(Index model_id)
{
    using namespace std;
    ModelData* model = m_manager->getModel(model_id);
    if (!model || !model->hasMesh()) {
        return {};
    }
    MeshData* md = model->asMeshData();

    // 构造 ModelData
    MeshDataVtk model_data { 
        md->solid_types_, md->solid_vertices_, md->solid_vertices_offset_,
        md->solid_faces_vertices_, md->solid_faces_vertices_offset_,
        md->solid_faces_, md->solid_faces_offset_,
        md->face_vertices_, md->face_vertices_offset_,
        md->edge_vertices_, md->vertex_attributes_,md->edge_attributes_,md->face_attributes_,md->solid_attributes_,{},-1 };

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
    Component* comp = m_manager->findComponent(component_id);
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

std::vector<std::array<double, 3>> QModelQuery::copyGlobalPoints() const
{
    return m_manager->globalPoints();
}

std::vector<SplineDataVtk> QModelQuery::getSplineData(Index model_id)
{
    std::vector<SplineDataVtk> result;

    ModelData* model = m_manager->getModel(model_id);

    auto& comps = model->components();
    for (const auto& comp : comps) {
        if (!comp || !comp->cad || !comp->cad->rootShape)
            continue;

        result.push_back(SplineDataVtk { *comp->cad->rootShape, comp->id });
    }

    return result;
}

std::optional<SplineDataVtk> QModelQuery::getSplineDataByComponent(Index component_id)
{
    Component* comp = m_manager->findComponent(component_id);
    if (!comp || !comp->cad || !comp->cad->rootShape)
        return std::nullopt;

    return SplineDataVtk { *comp->cad->rootShape, comp->id };
}


std::vector<Index> QModelQuery::getComponentIds(Index model_id) const
{
    return m_manager->getComponentIds(model_id);
}

QVariantList QModelQuery::getCadEdgeMappedPointIds(Index component_id, int localCadEdgeId)
{
    QVariantList out;

    Component* comp = m_manager->findComponent(component_id);
    if (!comp || !comp->cad)
        return out;

    auto gidOpt = resolveCadEdgeLocalId(component_id, localCadEdgeId);
    if (!gidOpt)
        return out;

    if (!comp->mapping)
        return out;

    auto it = comp->mapping->cad_edge_to_mesh_point_gids.find(*gidOpt);
    if (it == comp->mapping->cad_edge_to_mesh_point_gids.end())
        return out;

    for (Index pid : it->second)
        out.push_back(pid);
    return out;
}

QString QModelQuery::getModelName(Index model_id) const
{
    ModelData* model = m_manager->getModel(model_id);
    if (!model) {
        spdlog::error("模型不存在，无法获取名称,id:{}", model_id);
        return QString();
    }
    return QString::fromLocal8Bit(model->model_name_);
}

Q_INVOKABLE QStringList QModelQuery::getModelAttriName(Index model_id) const
{
    QStringList attri_list;
    ModelData* model = m_manager->getModel(model_id);
    if (!model) {
        spdlog::error("模型不存在，无法获取属性名，id:{}", model_id);
        return {};
    }
    MeshData* mesh = model->asMeshData();
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
        spdlog::info("attri_list.size():", attri_list.size());
    }
    return attri_list;
}

Q_INVOKABLE QList<Element::Type> QModelQuery::getModelAttriType(Index model_id) const
{
    QList<Element::Type> type_list;
    ModelData* model = m_manager->getModel(model_id);
    if (!model) {
        spdlog::error( "模型不存在，无法获取属性类型，id:{}",model_id);
        return {};
    }
    MeshData* mesh = model->asMeshData();
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
        spdlog::info ("type_list.size():" ,type_list.size());
    }
    return type_list;
}

//判断模型类型：mesh返回0，spline返回1，未知返回-1
int QModelQuery::getModelType(Index model_id) const
{
    if (m_manager->models_[model_id]->hasMesh())
        return 0;
    if (m_manager->models_[model_id]->hasSpline())
        return 1;
    return -1;
}

std::optional<GeomFaceId> QModelQuery::resolveCadFaceLocalId(Index component_id, int localFaceId)
{
    Component* comp = m_manager->findComponent(component_id);
    if (!comp || !comp->cad || !comp->cad->rootShape)
        return std::nullopt;

    comp->cad->ensureCadIndexBuilt(m_manager->geomRegistry());

    GeomFaceId gid = comp->cad->cad_index.faceGlobalId(localFaceId);
    if (gid == kInvalidGeomFaceId)
        return std::nullopt;

    return gid;
}

std::optional<GeomEdgeId> QModelQuery::resolveCadEdgeLocalId(Index component_id, int localEdgeId)
{
    Component* comp = m_manager->findComponent(component_id);
    if (!comp || !comp->cad || !comp->cad->rootShape)
        return std::nullopt;

    comp->cad->ensureCadIndexBuilt(m_manager->geomRegistry());

    GeomEdgeId gid = comp->cad->cad_index.edgeGlobalId(localEdgeId);
    if (gid == kInvalidGeomEdgeId)
        return std::nullopt;

    return gid;
}

std::optional<GeomVertexId> QModelQuery::resolveCadVertexLocalId(Index component_id, int localVertexId)
{
    Component* comp = m_manager->findComponent(component_id);
    if (!comp || !comp->cad || !comp->cad->rootShape)
        return std::nullopt;

    comp->cad->ensureCadIndexBuilt(m_manager->geomRegistry());

    GeomVertexId gid = comp->cad->cad_index.vertexGlobalId(localVertexId);
    if (gid == kInvalidGeomVertexId)
        return std::nullopt;

    return gid;
}

std::optional<GeomSolidId> QModelQuery::resolveCadSolidLocalId(Index component_id, int localSolidId)
{
    Component* comp = m_manager->findComponent(component_id);
    if (!comp || !comp->cad || !comp->cad->rootShape)
        return std::nullopt;

    comp->cad->ensureCadIndexBuilt(m_manager->geomRegistry());

    GeomSolidId gid = comp->cad->cad_index.solidGlobalId(localSolidId);
    if (gid == kInvalidGeomSolidId)
        return std::nullopt;

    return gid;
}