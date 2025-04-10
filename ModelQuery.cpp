#include "ModelQuery.h"
#include "Model.h"
#include <QVariantMap>
#include <QVariantList>
#include <QString>
#include <stdexcept>
#include <limits>

ModelQuery::ModelQuery(Model* model, QObject* parent)
    : QObject(parent), m_model(model)
{
    // 初始化时保存关联的 Model 指针
}

QVariantMap ModelQuery::getPatchInfo(int patchId) const {
    QVariantMap info;
    auto it = m_model->patches_.find(patchId);
    if (it == m_model->patches_.end()) {
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

QVariantList ModelQuery::getBlockList() const {
    QVariantList list;
    for (const auto& pair : m_model->blocks_) {
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

QVariantList ModelQuery::getPatchIds() const {
    QVariantList list;
    for (const auto& pair : m_model->patches_) {
        list.append(pair.first);
    }
    return list;
}

QVariantList ModelQuery::getBlockIds() const {
    QVariantList list;
    for (const auto& pair : m_model->blocks_) {
        list.append(pair.first);
    }
    return list;
}

QVariantList ModelQuery::getGroupIds() const {
    QVariantList list;
    for (const auto& pair : m_model->groups_) {
        list.append(pair.first);
    }
    return list;
}

QVariantMap ModelQuery::getPatchInfoByFaceId(int faceId) const {
    QVariantMap info;
    try {
        // 利用 Model 中已有的 face_patch_id 方法
        int patchId = m_model->face_patch_id(faceId);
        info = getPatchInfo(patchId);
    }
    catch (const std::exception& ex) {
        info["error"] = QString("No patch found for face %1: %2")
            .arg(faceId)
            .arg(ex.what());
    }
    return info;
}

QVariantMap ModelQuery::getBlockInfo(int blockId) const {
    QVariantMap info;
    auto it = m_model->blocks_.find(blockId);
    if (it == m_model->blocks_.end()) {
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

QVariantMap ModelQuery::getGroupInfo(int groupId) const {
    QVariantMap info;
    auto it = m_model->groups_.find(groupId);
    if (it == m_model->groups_.end()) {
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

QVariantList ModelQuery::queryPatchesByCondition(const QVariantMap& conditions) const {
    QVariantList results;
    // 支持条件：minVertexCount, maxFaceCount
    int minVertexCount = conditions.contains("minVertexCount") ? conditions.value("minVertexCount").toInt() : 0;
    int maxFaceCount = conditions.contains("maxFaceCount") ? conditions.value("maxFaceCount").toInt() : std::numeric_limits<int>::max();

    for (const auto& pair : m_model->patches_) {
        const auto& patch = pair.second;
        int vertexCount = static_cast<int>(patch->vertexIDs_.size());
        int faceCount = static_cast<int>(patch->faceIDs_.size());
        if (vertexCount >= minVertexCount && faceCount <= maxFaceCount) {
            results.append(getPatchInfo(pair.first));
        }
    }
    return results;
}

QVariantMap ModelQuery::getVertexInfo(int vertexId) const {
    QVariantMap info;
    bool found = false;
    // 遍历所有 Patch 查找该顶点
    for (const auto& pair : m_model->patches_) {
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