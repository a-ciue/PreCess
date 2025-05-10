#include "ModelQuery.h"

#include <QVariantList>
#include <QString>
#include <stdexcept>
#include <limits>

QModelQuery::Data QModelQuery::getMeshData(const QString& model_name)
{
    Data data{};
    auto it = m_manager->models_.find(model_name);
    if (it != m_manager->models_.end()) {
        data.patches = &it->second->patches_;
        data.blocks = &it->second->blocks_;
        data.group = &it->second->groups_;
    }
    return data;
}

QModelQuery::QModelQuery(ModelManager* mgr, QObject* parent)
        : QObject(parent), m_manager(mgr) {
}

QVariantMap QModelQuery::getPatchInfo(const QString& model_name, int patchId) const {
    ModelData* model = m_manager->getModel(model_name);
    if (!model) {
        return {};
    }

    QVariantMap info;
    auto it = model->patches_.find(patchId);
    if (it == model->patches_.end()) {
        info["error"] = QString("Patch %1 not found").arg(patchId);
        return info;
    }
    const auto& patch = it->second;
    info["id"] = patch->id_;
    info["blockID"] = patch->blockID;
    info["father_id"] = patch->father_id;

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

QVariantList QModelQuery::getBlockList(const QString& model_name) const
{
    QVariantList list;
    ModelData* model = m_manager->getModel(model_name);
    if (!model) {
        return list;
    }
    for (const auto& pair : model->blocks_) {
        const auto& block = pair.second;
        QVariantMap blockMap;
        blockMap["id"] = block->id;
        blockMap["groupID"] = block->groupID;

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

QVariantList QModelQuery::getPatchIds(const QString& model_name) const
{
    QVariantList list;
    ModelData* model = m_manager->getModel(model_name);
    if (!model) {
        return list;
    }
    for (const auto& pair : model->patches_) {
        list.append(pair.first);
    }
    return list;
}

QVariantList QModelQuery::getBlockIds(const QString& model_name) const
{
    QVariantList list;
    ModelData* model = m_manager->getModel(model_name);
    if (!model) {
        return list;
    }
    for (const auto& pair : model->blocks_) {
        list.append(pair.first);
    }
    return list;
}

QVariantList QModelQuery::getGroupIds(const QString& model_name) const
{
    QVariantList list;
    ModelData* model = m_manager->getModel(model_name);
    if (!model) {
        return list;
    }
    for (const auto& pair : model->groups_) {
        list.append(pair.first);
    }
    return list;
}

QVariantMap QModelQuery::getPatchInfoByFaceId(const QString& model_name, int faceId) const
{
    QVariantMap info;
    ModelData* model = m_manager->getModel(model_name);
    if (!model) {
        info["error"] = QString("Model not found");
        return info;
    }
    try {
        // 利用 ModelData 中已有的 face_patch_id 方法
        int patchId = model->face_patch_id(faceId);
        info = getPatchInfo(model_name, patchId);
    }
    catch (const std::exception& ex) {
        info["error"] = QString("No patch found for face %1: %2")
            .arg(faceId)
            .arg(ex.what());
    }
    return info;
}

QVariantMap QModelQuery::getBlockInfo(const QString& model_name, int blockId) const
{
    QVariantMap info;
    ModelData* model = m_manager->getModel(model_name);
    if (!model) {
        info["error"] = QString("Model not found");
        return info;
    }
    auto it = model->blocks_.find(blockId);
    if (it == model->blocks_.end()) {
        info["error"] = QString("Block %1 not found").arg(blockId);
        return info;
    }
    const auto& block = it->second;
    info["id"] = block->id;
    info["groupID"] = block->groupID;
    QVariantList patchIDs;
    for (int pid : block->patchIDs) {
        patchIDs.append(pid);
    }
    info["patchIDs"] = patchIDs;
    return info;
}

QVariantMap QModelQuery::getGroupInfo(const QString& model_name, int groupId) const
{
    QVariantMap info;
    ModelData* model = m_manager->getModel(model_name);
    if (!model) {
        info["error"] = QString("Model not found");
        return info;
    }
    auto it = model->groups_.find(groupId);
    if (it == model->groups_.end()) {
        info["error"] = QString("Group %1 not found").arg(groupId);
        return info;
    }
    const auto& group = it->second;
    info["id"] = group->id;
    QVariantList blockIDs;
    for (int bid : group->blockIDs) {
        blockIDs.append(bid);
    }
    info["blockIDs"] = blockIDs;
    return info;
}

QVariantList QModelQuery::queryPatchesByCondition(const QString& model_name, const QVariantMap& conditions) const
{
    QVariantList results;
    ModelData* model = m_manager->getModel(model_name);
    if (!model) {
        return results;
    }
    
    int minVertexCount = conditions.contains("minVertexCount") ? conditions.value("minVertexCount").toInt() : 0;
    int maxFaceCount = conditions.contains("maxFaceCount") ? conditions.value("maxFaceCount").toInt() : std::numeric_limits<int>::max();

    for (const auto& pair : model->patches_) {
        const auto& patch = pair.second;
        int vertexCount = static_cast<int>(patch->vertexIDs_.size());
        int faceCount = static_cast<int>(patch->faceIDs_.size());
        if (vertexCount >= minVertexCount && faceCount <= maxFaceCount) {
            results.append(getPatchInfo(model_name, pair.first));
        }
    }
    return results;
}

QVariantMap QModelQuery::getVertexInfo(const QString& model_name, int vertexId) const
{
    QVariantMap info;
    ModelData* model = m_manager->getModel(model_name);
    if (!model) {
        info["error"] = QString("Model not found");
        return info;
    }

    bool found = false;
    // 遍历所有 Patch 查找该顶点
    for (const auto& pair : model->patches_) {
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

ModelData* QModelQuery::getModelData(const QString& model_name) const
{
    auto it = m_manager->models_.find(model_name);
    if (it != m_manager->models_.end()) {
        return it->second.get();
    }
    return nullptr;
}