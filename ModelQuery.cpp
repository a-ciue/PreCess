#include "ModelQuery.h"

#include <QVariantList>
#include <QString>
#include <stdexcept>
#include <limits>

QModelQuery::QModelQuery(ModelManager* mgr, QObject* parent)
        : QObject(parent), m_manager(mgr) {
}

std::optional<MeshDataVtk> QModelQuery::getModelData(Index model_id)
{
    using namespace std;
    ModelData* model = m_manager->getModel(model_id);
    if (!model || !model->isMesh()) {
        return {};
    }
    MeshData* md = model->asMeshData();

    // 构造 ModelData
    MeshDataVtk model_data { md->face_vertices, md->vertex_positions, {} };

    // 添加所有块
    BlockDatas block_datas;
    for (const auto& [block_id, block] : md->blocks_) {
        BlockData block_data;
        block_data.id = block_id;

        // 添加该块中所有的patch faces
        vector<Index>& block_faces = block_data.faces_;
        for (const auto& patch_id : block->patchIDs) {
            vector<Index> patch_faces = md->patches_[patch_id]->faces;
            block_faces.insert(block_faces.end(), patch_faces.begin(), patch_faces.end());
        }

        block_datas.block_datas.push_back(block_data);
    }
    model_data.model_blocks_ = block_datas;

    return model_data;
}

std::optional<SplineDataVtk> QModelQuery::getSplineData(Index model_id)
{
    std::optional<SplineDataVtk> model_data{};
    ModelData* model = m_manager->getModel(model_id);
    if (model->isSpline())
    {
        model_data = model->asSplineData()->getSplineData();
    }
    return model_data;
}

QString QModelQuery::getModelName(Index model_id) const
{
    ModelData* model = m_manager->getModel(model_id);
    if (!model) {
        qWarning() << "模型不存在，无法获取名称:" << model_id;
        return QString();
    }
    return QString::fromStdString(model->model_name_);
}

//判断模型类型：mesh返回0，spline返回1，未知返回-1
int QModelQuery::getModelType(Index model_id) const
{
    if (m_manager->models_[model_id]->isMesh())
        return 0;
    if (m_manager->models_[model_id]->isSpline())
        return 1;
    return -1;
}

