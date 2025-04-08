#pragma once

#include <QObject>
#include <QVariant>
#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QQmlEngine>  // 提供 QML 元素导出宏 (Qt6)
#endif

// 前向声明 Model 类
class Model;

/**
 * ModelQuery 类封装了 Model 的所有查询操作（CQRS 查询部分）。
 * 通过将查询逻辑从 Model 的命令操作中分离，实现读写分离。
 * 初始版本将所有查询接口集中在此类中，将来可按需细分为 PatchQuery、BlockQuery 等。
 * ModelQuery 可以访问 Model 的私有数据（Model 将其声明为友元类），以获取所需信息。
 * 查询方法通过 Q_INVOKABLE 暴露给 QML，返回 QVariant/QVariantMap/QVariantList 结果。
 */
class ModelQuery : public QObject {
    Q_OBJECT
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QML_ELEMENT  // Qt6+: 导出为 QML 可用类型（Qt5 请使用 qmlRegisterType）
#endif

public:
    /**
     * @brief 构造函数
     * @param model 指向关联的 Model 实例，该查询对象将读取其内部数据
     * @param parent 父对象（默认 nullptr），建议设置为 Model 以便内存自动管理
     */
    explicit ModelQuery(Model* model, QObject* parent = nullptr);

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
     * @param patchId Patch 的标识符
     * @return 包含 Patch 详细信息的 QVariantMap
     */
    Q_INVOKABLE QVariantMap getPatchInfo(int patchId) const;

    /**
     * @brief 获取所有 Block 的列表信息
     *
     * 遍历 m_model->blocks_，将每个 Block 内的信息转换为 QVariantMap，
     * 并将所有 Block 的信息存入 QVariantList 返回。
     *
     * @details
     * 每个 Block 产生的 QVariantMap 包含以下键：
     * - "id": Block 的 id
     * - "groupID": 所属 group 的 id
     * - "patchIDs": 包含的 Patch id 列表 (QVariantList)
     *
     * @return 包含多个 Block 详细信息的 QVariantList，每个元素为 QVariantMap
     */
    Q_INVOKABLE QVariantList getBlockList() const;

    /**
     * @brief 获取所有 Patch 的 ID 列表
     * @return 包含所有 Patch 标识符的 QVariantList
     */
    Q_INVOKABLE QVariantList getPatchIds() const;

    /**
     * @brief 获取所有 Block 的 ID 列表
     * @return 包含所有 Block 标识符的 QVariantList
     */
    Q_INVOKABLE QVariantList getBlockIds() const;

    /**
     * @brief 获取所有 Group 的 ID 列表
     * @return 包含所有 Group 标识符的 QVariantList
     */
    Q_INVOKABLE QVariantList getGroupIds() const;

    /**
     * @brief 根据给定 Face ID 获取所在 Patch 的详细信息
     * @param faceId Face 的标识符
     * @return 包含所在 Patch 详细信息的 QVariantMap，如果无效则包含 error 信息
     */
    Q_INVOKABLE QVariantMap getPatchInfoByFaceId(int faceId) const;

    /**
     * @brief 获取指定 Block 的详细信息
     * @param blockId Block 的标识符
     * @return 包含 Block 详细信息的 QVariantMap，如果无效则包含 error 信息
     */
    Q_INVOKABLE QVariantMap getBlockInfo(int blockId) const;

    /**
     * @brief 获取指定 Group 的详细信息
     * @param groupId Group 的标识符
     * @return 包含 Group 详细信息的 QVariantMap，如果无效则包含 error 信息
     */
    Q_INVOKABLE QVariantMap getGroupInfo(int groupId) const;

    /**
     * @brief 根据指定条件查询满足条件的 Patch 列表
     *
     * 支持条件键例如："minVertexCount"（最少顶点数量）和 "maxFaceCount"（最多面数）。
     *
     * @param conditions 查询条件构成的 QVariantMap
     * @return 符合条件的 Patch 详细信息列表，每个元素为 QVariantMap
     */
    Q_INVOKABLE QVariantList queryPatchesByCondition(const QVariantMap& conditions) const;

    /**
     * @brief 获取指定顶点的详细信息
     *
     * 由于顶点信息分散于各 Patch 中，返回第一次找到的匹配信息。
     *
     * @param vertexId 顶点的标识符
     * @return 包含顶点详细信息的 QVariantMap，如 vertexId、坐标、所在 Patch 等；如果未找到则包含 error 信息
     */
    Q_INVOKABLE QVariantMap getVertexInfo(int vertexId) const;

private:
    Model* m_model;  ///< 关联的 Model 实例，用于访问其数据
};
