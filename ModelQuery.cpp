#include "ModelQuery.h"

#include <QVariantList>
#include <QString>
#include <stdexcept>
#include <limits>

std::optional<MeshDataVtk> QModelQuery::getModelData(Index model_id)
{
    ModelData* model = m_manager->getModel(model_id);
    if (!model || !model->isMesh()) {
        return {};
    }
    MeshData* md = model->asMeshData();

    // 构造 ModelData
    MeshDataVtk modelData;

    // 添加所有顶点和三角形
    int offset {};
    unordered_map<int, vector<int>> patch_vtk_face_ids;
    for (const auto& [patch_id, patch] : md->patches_) {
        // 添加顶点和模型顶点ID
        modelData.vtk_points_.insert(modelData.vtk_points_.end(), patch->vertexPoints_.begin(), patch->vertexPoints_.end());
        modelData.model_point_id_.insert(modelData.model_point_id_.end(), patch->vertexIDs_.begin(), patch->vertexIDs_.end());
        modelData.model_face_id_.insert(modelData.model_face_id_.end(), patch->faceIDs_.begin(), patch->faceIDs_.end());

        // 添加三角形和模型面ID
        for (size_t i = 0; i < patch->faceTriangles_.size(); ++i) {
            array<Index, 3> arr;
            arr[0] = patch->faceTriangles_[i][0] + offset;
            arr[1] = patch->faceTriangles_[i][1] + offset;
            arr[2] = patch->faceTriangles_[i][2] + offset;
            modelData.vtk_triangles_.push_back(arr);
            patch_vtk_face_ids[patch_id].push_back(modelData.vtk_triangles_.size() - 1);
        }
        offset += patch->vertexPoints_.size();
    }

    // 添加所有块
    BlockDatas blockDatas;
    for (const auto& [block_id, block] : md->blocks_) {
        BlockData blockData;
        blockData.model_id_ = block_id;
        // 添加该块中所有的patch
        for (const auto& patch_id : block->patchIDs) {
            vector<int>& vtk_face_ids = patch_vtk_face_ids[patch_id];
            for (int vtk_face_id : vtk_face_ids) {
                blockData.faces_.push_back(vtk_face_id);
            }
        }
        blockDatas.block_datas.push_back(blockData);
    }
    modelData.model_blocks_ = blockDatas;

    return modelData;
}

std::optional<SplineDataVtk> QModelQuery::getSplineData(Index model_id)
{
    std::optional<SplineDataVtk> model_data{};
    ModelData* model = m_manager->getModel(model_id);
    if (model->isSpline())
    {
        model_data = model->getSplineData();
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
    return model->getModelName();
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

QModelQuery::QModelQuery(ModelManager* mgr, QObject* parent)
        : QObject(parent), m_manager(mgr) {
}

QVariantMap QModelQuery::getPatchInfo(Index model_id, int patchId) const {
    ModelData* model = m_manager->getModel(model_id);
    if (!model) {
        return {};
    }
    MeshData* mesh = model->asMeshData();
    QVariantMap info;
    auto it = mesh->patches_.find(patchId);
    if (it == mesh->patches_.end()) {
        info["error"] = QString("Patch %1 not found").arg(patchId);
        return info;
    }
    const auto& patch = it->second;
    info["id"] = patch->id_;
    info["blockID"] = patch->blockID;

    // 转换 faceIDs_
    QVariantList faceIDsList;
    for (int faceId : patch->faceIDs_) {
        faceIDsList.append(faceId);
    }
    info["faceIDs"] = faceIDsList;

    // 转换 faceTriangles_（每个为 std::array<int,3>）
    QVariantList trianglesList;
    for (const auto& triangle : patch->faceTriangles_) {
        QVariantList tri;
        for (int j = 0; j < 3; ++j) {
            tri.append(triangle[j]);
        }
        trianglesList.append(tri);
    }
    info["faceTriangles"] = trianglesList;

    // 转换 vertexIDs_
    QVariantList vertexIDsList;
    for (int vid : patch->vertexIDs_) {
        vertexIDsList.append(vid);
    }
    info["vertexIDs"] = vertexIDsList;

    // 转换 vertexPoints_（每个为 std::array<double,3>）
    QVariantList vertexPointsList;
    for (const auto& point : patch->vertexPoints_) {
        QVariantList pt; // 子列表
        for (int j = 0; j < 3; ++j) {
            double coord = point[j]; // 显式提取坐标值
            pt.append(coord); // 添加坐标到子列表
       }

        // 关键修复：使用 append 添加整个子列表，显式转换为 QVariant
        vertexPointsList.append(QVariant(pt));
    }
    info["vertexPoints"] = vertexPointsList;

    return info;
}

QVariantList QModelQuery::getBlockList(Index model_id) const
{
    QVariantList list;
    ModelData* model = m_manager->getModel(model_id);
    if (!model) {
        return list;
    }
    MeshData* mesh = model->asMeshData();
    for (const auto& pair : mesh->blocks_) {
        const auto& block = pair.second;
        QVariantMap blockMap;
        blockMap["id"] = block->id;

        // 转换 patchIDs（unordered_set<int>）
        QVariantList patchIDs;
        for (int pid : block->patchIDs) {
            patchIDs.append(pid);
        }
        blockMap["patchIDs"] = patchIDs;
        list.append(QVariant(blockMap));
    }
    return list;
}

QVariantList QModelQuery::getPatchIds(Index model_id) const
{
    QVariantList list;
    ModelData* model = m_manager->getModel(model_id);
    MeshData* mesh = model->asMeshData();
    if (!model) {
        return list;
    }
    for (const auto& pair : mesh->patches_) {
        list.append(pair.first);
    }
    return list;
}

QVariantList QModelQuery::getBlockIds(Index model_id) const
{
    QVariantList list;
    ModelData* model = m_manager->getModel(model_id);
    MeshData* mesh = model->asMeshData();
    if (!model) {
        return list;
    }
    for (const auto& pair : mesh->blocks_) {
        list.append(pair.first);
    }
    return list;
}

QVariantMap QModelQuery::getPatchInfoByFaceId(Index model_id, int faceId) const
{
    QVariantMap info;
    ModelData* model = m_manager->getModel(model_id);
    if (!model) {
        info["error"] = QString("Model not found");
        return info;
    }
    try {
        // 利用 ModelData 中已有的 face_patch_id 方法
        int patchId = model->face_patch_id(faceId);
        info = getPatchInfo(model_id, patchId);
    }
    catch (const std::exception& ex) {
        info["error"] = QString("No patch found for face %1: %2")
            .arg(faceId)
            .arg(ex.what());
    }
    return info;
}

QVariantMap QModelQuery::getBlockInfo(Index model_id, int blockId) const
{
    QVariantMap info;
    ModelData* model = m_manager->getModel(model_id);
    MeshData* mesh = model->asMeshData();
    if (!model) {
        info["error"] = QString("Model not found");
        return info;
    }
    auto it = mesh->blocks_.find(blockId);
    if (it == mesh->blocks_.end()) {
        info["error"] = QString("Block %1 not found").arg(blockId);
        return info;
    }
    const auto& block = it->second;
    info["id"] = block->id;
    QVariantList patchIDs;
    for (int pid : block->patchIDs) {
        patchIDs.append(pid);
    }
    info["patchIDs"] = patchIDs;
    return info;
}

QVariantList QModelQuery::queryPatchesByCondition(Index model_id, const QVariantMap& conditions) const
{
    QVariantList results;
    ModelData* model = m_manager->getModel(model_id);
    MeshData* mesh = model->asMeshData();
    if (!model) {
        return results;
    }
    
    int minVertexCount = conditions.contains("minVertexCount") ? conditions.value("minVertexCount").toInt() : 0;
    int maxFaceCount = conditions.contains("maxFaceCount") ? conditions.value("maxFaceCount").toInt() : std::numeric_limits<int>::max();

    for (const auto& pair : mesh->patches_) {
        const auto& patch = pair.second;
        int vertexCount = static_cast<int>(patch->vertexIDs_.size());
        int faceCount = static_cast<int>(patch->faceIDs_.size());
        if (vertexCount >= minVertexCount && faceCount <= maxFaceCount) {
            results.append(getPatchInfo(model_id, pair.first));
        }
    }
    return results;
}

QVariantMap QModelQuery::getVertexInfo(Index model_id, int vertexId) const
{
    QVariantMap info;
    ModelData* model = m_manager->getModel(model_id);
    MeshData* mesh = model->asMeshData();
    if (!model) {
        info["error"] = QString("Model not found");
        return info;
    }

    bool found = false;
    // 遍历所有 Patch 查找该顶点
    for (const auto& pair : mesh->patches_) {
        const auto& patch = pair.second;
        const auto& vertexIDs = patch->vertexIDs_;
        const auto& vertexPoints = patch->vertexPoints_;
        for (size_t i = 0; i < vertexIDs.size(); ++i) {
            if (vertexIDs[i] == vertexId) {
                info["vertexId"] = vertexId;
                QVariantList coordinates;
                coordinates.append(vertexPoints[i][0]);
                coordinates.append(vertexPoints[i][1]);
                coordinates.append(vertexPoints[i][2]);
                info["coordinates"] = coordinates;
                info["patchId"] = patch->id_;
                found = true;
                break;
            }
        }
        if (found) break;
    }
    if (!found) {
        info["error"] = QString("Vertex %1 not found").arg(vertexId);
    }
    return info;
}
