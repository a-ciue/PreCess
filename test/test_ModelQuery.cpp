//
// Created by 徐昊阳 on 4/8/25.
//
// test_ModelQuery.cpp
#include "gtest/gtest.h"
#include "QModelQuery.h"
#include "ModelLayer.h"
#include <QVariant>

class ModelQueryTest : public ::testing::Test {
protected:
    void SetUp() override {
        query = new QModelQuery(&manager_, nullptr);
    }

    void TearDown() override {
        delete query;
    }

    ModelLayer manager_;
    QModelQuery* query;
    const QString model_name_{"dummy"};
};

TEST_F(ModelQueryTest, GetPatchInfoValid) {
    // 测试 getPatchInfo 接口，使用已填充的数据 patch id = 1
    QVariantMap patchInfo;
    EXPECT_EQ(patchInfo.value("id").toInt(), 1);
    EXPECT_EQ(patchInfo.value("blockID").toInt(), 1);
    EXPECT_EQ(patchInfo.value("father_id").toInt(), 101);

    // 检查 faceIDs 列表
    QVariantList faceIDs = patchInfo.value("faceIDs").toList();
    ASSERT_EQ(faceIDs.size(), 2);
    EXPECT_EQ(faceIDs.at(0).toInt(), 10);
    EXPECT_EQ(faceIDs.at(1).toInt(), 11);

    // 检查 vertexPoints（每个点为 QList<double>）
    QVariantList vertexPoints = patchInfo.value("vertexPoints").toList();
    ASSERT_EQ(vertexPoints.size(), 3);
    for (const auto& pointVariant : vertexPoints) {
        QVariantList point = pointVariant.toList();
        ASSERT_EQ(point.size(), 3); // 每个子列表应有3个坐标值
    }
    // 可以进一步验证每个点的数值
}

TEST_F(ModelQueryTest, GetPatchInfoInvalid) {
    // 对不存在的 patchId 调用 getPatchInfo，检查 error 字段是否返回
    QVariantMap patchInfo;
    EXPECT_TRUE(patchInfo.contains("error"));
}

TEST_F(ModelQueryTest, GetBlockList) {
    QVariantList blockList;
    // 预期有3个 block
	EXPECT_EQ(blockList.size(), 3);
    // 验证第一个 block 的 id 是否正确
    QVariantMap block0 = blockList.at(0).toMap();
    EXPECT_EQ(block0.value("id").toInt(), 3);
    QVariantMap block1 = blockList.at(1).toMap();
    EXPECT_EQ(block1.value("id").toInt(), 2);
    QVariantMap block2 = blockList.at(2).toMap();
    EXPECT_EQ(block2.value("id").toInt(), 1);
}

TEST_F(ModelQueryTest, GetPatchIds) {
    QVariantList patchIds;
    EXPECT_EQ(patchIds.size(), 3);
    // 验证包含 1, 2, 3
    std::set<int> idSet;
    for (const QVariant &v : patchIds)
        idSet.insert(v.toInt());
    EXPECT_EQ(idSet, std::set<int>({1, 2, 3}));
}

TEST_F(ModelQueryTest, GetBlockIds) {
    QVariantList blockIds;
    EXPECT_EQ(blockIds.size(), 3);
    std::set<int> idSet;
    for (const QVariant &v : blockIds)
        idSet.insert(v.toInt());
    EXPECT_EQ(idSet, std::set<int>({1, 2, 3}));
}

TEST_F(ModelQueryTest, GetGroupIds) {
    QVariantList groupIds;
    EXPECT_EQ(groupIds.size(), 3);
    std::set<int> idSet;
    for (const QVariant &v : groupIds)
        idSet.insert(v.toInt());
    EXPECT_EQ(idSet, std::set<int>({1, 2, 3}));
}

TEST_F(ModelQueryTest, GetPatchInfoByFaceIdValid) {
    // 假设 dummy patch 1 包含 faceIDs {10, 11}，因此查询 faceId = 10 应返回 patch1 的数据
    QVariantMap patchInfo;
    EXPECT_EQ(patchInfo.value("id").toInt(), 1);
}

TEST_F(ModelQueryTest, GetBlockInfoValid) {
    QVariantMap blockInfo;
    EXPECT_EQ(blockInfo.value("id").toInt(), 2);
    EXPECT_EQ(blockInfo.value("groupID").toInt(), 202);
    // 检查 patchIDs 列表应包含 2
    QVariantList patchIDs = blockInfo.value("patchIDs").toList();
    EXPECT_EQ(patchIDs.size(), 1);
    EXPECT_EQ(patchIDs.at(0).toInt(), 2);
}

TEST_F(ModelQueryTest, GetGroupInfoValid) {
    QVariantMap groupInfo;
    EXPECT_EQ(groupInfo.value("id").toInt(), 3);
    QVariantList blockIDs = groupInfo.value("blockIDs").toList();
    EXPECT_EQ(blockIDs.size(), 1);
    EXPECT_EQ(blockIDs.at(0).toInt(), 3);
}

TEST_F(ModelQueryTest, QueryPatchesByCondition) {
    // 测试条件查询，例如：minVertexCount = 3, maxFaceCount = 3
    QVariantMap conditions;
    conditions.insert("minVertexCount", 3);
    conditions.insert("maxFaceCount", 3);
    QVariantList result;
    // 在 dummy 数据中，所有 patch 的 vertex 数量均为3且 face 数量为2，
    // 因此符合 face count <= 3 的条件，预期全部3个 patch 返回
	EXPECT_EQ(result.size(), 3);
}

TEST_F(ModelQueryTest, GetVertexInfoValid) {
    // 测试：在 dummy patch1 中，vertexIDs 包含 100,101,102；我们查询 101 应返回 patch1 的数据
    QVariantMap vertexInfo;
    EXPECT_EQ(vertexInfo.value("vertexId").toInt(), 101);
    EXPECT_EQ(vertexInfo.value("patchId").toInt(), 1);
}

TEST_F(ModelQueryTest, GetVertexInfoInvalid) {
    QVariantMap vertexInfo;
    EXPECT_TRUE(vertexInfo.contains("error"));
}
