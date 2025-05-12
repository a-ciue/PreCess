/**
 * @file ModelQuery.h
 * @brief 封装 ModelData 查询操作接口，支持对网格数据进行各种查询
 *
 * QModelQuery 类提供了一系列接口用于从 ModelData 中查询网格数据，
 * 包括 Patch、Block、Group 和 Vertex 等信息的读取。通过将查询逻辑从 ModelData 的命令操作中分离，
 * 实现了读写分离。该类通过 Q_INVOKABLE 方法暴露给 QML 层使用，返回的结果以 QVariantMap 或 QVariantList 形式呈现，
 * 便于前端快速获得并展示数据。
 *
 * @author 徐昊阳 haoyangxu06@gmail.com
 * @date 2025/4/11
 */
#pragma once

#include <QObject>
#include <QVariant>
#include <QtGlobal>
#include <QQmlEngine>  // 提供 QML 元素导出宏 (Qt6)

#include "ModelManager.h"

// 前向声明 ModelData 类
class ModelData;

class IModelQuery {
public:
    virtual ~IModelQuery() = default;

    virtual std::optional<ModelDataVtk> getModelData(const QString& model_name) = 0;
};

/**
 * @brief ModelQuery 类封装所有网格数据的查询操作（CQRS 查询部分）
 *
 * 通过将查询逻辑与 ModelData 的命令操作分离，QModelQuery 实现了读写分离，专注于数据的查询。
 * 该类可以直接访问 ModelData 的私有数据（因为 ModelData 声明其为友元类），
 * 并以 Q_INVOKABLE 方法暴露各个查询接口给 QML 使用。
 */
class QModelQuery : public QObject, IModelQuery {
    Q_OBJECT
    QML_ELEMENT  // Qt6+: 导出为 QML 可用类型（Qt5 请使用 qmlRegisterType）

public:
    /**
     * @brief 构造函数
     *
     * @param mgr 指向 ModelManager 实例，用于管理并查找多个 ModelData
     * @param parent 父 QObject（默认为 nullptr），可用于 Qt 对象树内存管理
     */
    explicit QModelQuery(ModelManager* mgr, QObject* parent = nullptr);

    std::optional<ModelDataVtk> getModelData(const QString& model_name) override;

    /**
     * @brief 获取指定 Patch 的详细信息
     * 返回的 QVariantMap 包含的键：
     * - "id": patch 的 id
     * - "blockID": 所属 block 的 id
     * - "father_id": 父节点 id
     * - "faceIDs": 包含该 patch 的面 id 列表 (QVariantList)
     * - "faceTriangles": 每个面三角形的顶点索引 (QVariantList，每个元素也是 QList)
     * - "vertexIDs": 包含的顶点 id 列表 (QVariantList)
     * - "vertexPoints": 对应顶点的坐标 (QVariantList，每个元素为包含 3 个 double 值的 QList)
     *
     * 如果未能找到指定 patch，则返回包含 error 信息的 QVariantMap。
     *
     * @param model_name 具体要查询的模型名称，通过此名称在 ModelManager 中定位对应的 ModelData
     * @param patchId Patch 的标识符
     * @return 包含 Patch 详细信息的 QVariantMap
     */
    Q_INVOKABLE QVariantMap getPatchInfo(const QString& model_name, int patchId) const;

    /**
     * @brief 获取所有 Block 的列表信息
     *
     * 遍历 m_model->blocks_，将每个 Block 内的信息转换为 QVariantMap，
     * 并将所有 Block 的信息存入 QVariantList 返回。
     * 注意 Block 的顺序存储会颠倒，但是由于所有操作基于 Block 自身id，所以颠倒返回的顺序没有影响。
     *
     * @details
     * 每个 Block 产生的 QVariantMap 包含以下键：
     * - "id": Block 的 id
     * - "groupID": 所属 group 的 id
     * - "patchIDs": 包含的 Patch id 列表 (QVariantList)
     *
     * @param model_name 具体要查询的模型名称，通过此名称在 ModelManager 中定位对应的 ModelData
     * @return 包含多个 Block 详细信息的 QVariantList，每个元素为 QVariantMap
     */
    Q_INVOKABLE QVariantList getBlockList(const QString& model_name) const;

    /**
     * @brief 获取所有 Patch 的 ID 列表
     * @param model_name 具体要查询的模型名称，通过此名称在 ModelManager 中定位对应的 ModelData
     * @return 包含所有 Patch 标识符的 QVariantList
     */
    Q_INVOKABLE QVariantList getPatchIds(const QString& model_name) const;

    /**
     * @brief 获取所有 Block 的 ID 列表
     *
     * 遍历 ModelData 中所有 Patch，将每个 Patch 的标识符提取出来，
     * 并存入 QVariantList 返回。
     *
     * @param model_name 具体要查询的模型名称，通过此名称在 ModelManager 中定位对应的 ModelData
     * @return 包含所有 Block 标识符的 QVariantList
     */
    Q_INVOKABLE QVariantList getBlockIds(const QString& model_name) const;

    /**
     * @brief 获取所有 Group 的 ID 列表
     * @param model_name 具体要查询的模型名称，通过此名称在 ModelManager 中定位对应的 ModelData
     * @return 包含所有 Group 标识符的 QVariantList
     */
    Q_INVOKABLE QVariantList getGroupIds(const QString& model_name) const;

    /**
     * @brief 根据给定 Face ID 获取所在 Patch 的详细信息
     * @param model_name 具体要查询的模型名称，通过此名称在 ModelManager 中定位对应的 ModelData
     * @param faceId Face 的标识符
     * @return 包含所在 Patch 详细信息的 QVariantMap，如果无效则包含 error 信息
     */
    Q_INVOKABLE QVariantMap getPatchInfoByFaceId(const QString& model_name, int faceId) const;

    /**
     * @brief 获取指定 Block 的详细信息
     * @param model_name 具体要查询的模型名称，通过此名称在 ModelManager 中定位对应的 ModelData
     * @param blockId Block 的标识符
     * @return 包含 Block 详细信息的 QVariantMap，如果无效则包含 error 信息
     */
    Q_INVOKABLE QVariantMap getBlockInfo(const QString& model_name, int blockId) const;

    /**
     * @brief 获取指定 Group 的详细信息
     * @param model_name 具体要查询的模型名称，通过此名称在 ModelManager 中定位对应的 ModelData
     * @param groupId Group 的标识符
     * @return 包含 Group 详细信息的 QVariantMap，如果无效则包含 error 信息
     */
    Q_INVOKABLE QVariantMap getGroupInfo(const QString& model_name, int groupId) const;

    /**
     * @brief 根据指定条件查询满足条件的 Patch 列表
     *
     * 支持条件键例如："minVertexCount"（最少顶点数量）和 "maxFaceCount"（最多面数）。
     *
     * @param model_name 具体要查询的模型名称，通过此名称在 ModelManager 中定位对应的 ModelData
     * @param conditions 查询条件构成的 QVariantMap
     * @return 符合条件的 Patch 详细信息列表，每个元素为 QVariantMap
     */
    Q_INVOKABLE QVariantList queryPatchesByCondition(const QString& model_name, const QVariantMap& conditions) const;

    /**
     * @brief 获取指定顶点的详细信息
     *
     * 由于顶点信息分散于各 Patch 中，返回第一次找到的匹配信息。
     *
     * @param model_name 具体要查询的模型名称，通过此名称在 ModelManager 中定位对应的 ModelData
     * @param vertexId 顶点的标识符
     * @return 包含顶点详细信息的 QVariantMap，如 vertexId、坐标、所在 Patch 等；如果未找到则包含 error 信息
     */
    Q_INVOKABLE QVariantMap getVertexInfo(const QString& model_name, int vertexId) const;

private:
    ModelManager* m_manager;
};
