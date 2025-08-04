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
#include "MeshData.h"
#include "SplineData.h"
#include "SplineDataVtk.h"

// 前向声明 ModelData 类
class ModelData;

class IModelQuery {
public:
    virtual ~IModelQuery() = default;

    virtual std::optional<MeshDataVtk> getModelData(Index model_id) = 0;
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

    std::optional<MeshDataVtk> getModelData(Index model_id) override;

    std::optional<SplineDataVtk> getSplineData(Index model_id) ;

    Q_INVOKABLE QString getModelName(Index model_id) const;

    int getModelType(Index model_id) const;

private:
    ModelManager* m_manager;
};
