#include "QModelQuery.h"
#include "ModelManager.h"
#include "MeshData.h"
#include "SplineData.h"
#include "SplineDataVtk.h"

#include <QVariantList>
#include <QString>
#include <stdexcept>
#include <limits>
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
    MeshDataVtk model_data { md->vertex_positions_,
        md->solid_types_, md->solid_vertices_, md->solid_vertices_offset_,
        md->solid_faces_vertices_, md->solid_faces_vertices_offset_,
        md->solid_faces_, md->solid_faces_offset_,
        md->face_vertices_, md->face_vertices_offset_,
        md->edge_vertices_, md->vertex_attributes_,md->edge_attributes_,md->face_attributes_,md->solid_attributes_,{} };

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

std::optional<SplineDataVtk> QModelQuery::getSplineData(Index model_id)
{
    ModelData* model = m_manager->getModel(model_id);
    SplineData* data = model->asSplineData();

    return data ? data->getSplineData() : std::nullopt;
}

QString QModelQuery::getModelName(Index model_id) const
{
    ModelData* model = m_manager->getModel(model_id);
    if (!model) {
        qWarning() << "模型不存在，无法获取名称:" << model_id;
        return QString();
    }
    return QString::fromLocal8Bit(model->model_name_);
}

Q_INVOKABLE QStringList QModelQuery::getModelAttriName(Index model_id) const
{
    ModelData* model = m_manager->getModel(model_id);
    MeshData* mesh = model->asMeshData();
    if (mesh) {
        QStringList attri_list;
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
        return attri_list;
    }
}

Q_INVOKABLE QList<Element::Type> QModelQuery::getModelAttriType(Index model_id) const
{
    ModelData* model = m_manager->getModel(model_id);
    MeshData* mesh = model->asMeshData();
    if (mesh) {
        QList<Element::Type> type_list;
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
        return type_list;
    }
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

