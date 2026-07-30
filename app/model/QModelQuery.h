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
#include "Core.h"

#include <QObject>
#include <QVariant>
#include <QtGlobal>
#include <QQmlEngine> // 提供 QML 元素导出宏 (Qt6)
#include "QSelection.h"


struct GeometryDataVtk;
struct MeshDataVtk;
class ModelLayer;

class IModelQuery {
public:
    virtual ~IModelQuery() = default;

    virtual std::optional<MeshDataVtk> getMeshData(Index model_id) = 0;
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
    QML_ELEMENT // Qt6+: 导出为 QML 可用类型（Qt5 请使用 qmlRegisterType）

public :
    /**
     * @brief 构造函数
     *
     * @param mgr 指向 ModelLayer 实例，用于管理并查找多个 ModelData
     * @param parent 父 QObject（默认为 nullptr），可用于 Qt 对象树内存管理
     */
    explicit QModelQuery(ModelLayer* mgr, QObject* parent = nullptr);

    std::optional<MeshDataVtk> getMeshData(Index model_id) override;
    std::optional<MeshDataVtk> getMeshDataByComponent(Index component_id);

    /**
     * @brief 按两端点反查组件网格边的稳定局部 id
     * @param component_id 组件 ID
     * @param p0 边端点 id（与 MeshData 连通性同一键空间，当前为全局点 id）
     * @param p1 边另一端点 id（与 p0 无序）
     * @return 命中返回稳定局部边 id（跨拓扑编辑有效，物化边与面边同表）；
     *         未命中、无网格或数据异常返回 std::nullopt
     */
    std::optional<Index> findEdgeByEndpoints(Index component_id, Index p0, Index p1);

    const std::vector<std::array<double, 3>>& globalPoints() const;

    std::vector<GeometryDataVtk> getGeometryVtkData(Index model_id);
    std::optional<GeometryDataVtk> getGeometryVtkDataByComponent(Index component_id);

    std::vector<Index> getComponentIds(Index model_id) const;
    Q_INVOKABLE int findModelIdByComponent(Index component_id) const;
    /**
     * @brief 判断指定 id 的模型是否仍然存在
     * @param model_id 模型 ID
     */
    Q_INVOKABLE bool hasModel(Index model_id) const;
    /**
     * @brief 判断指定 id 的组件是否仍然存在
     * @param component_id 组件 ID
     */
    Q_INVOKABLE bool hasComponent(Index component_id) const;

    Q_INVOKABLE QVariantList getGeometryEdgeMappedPointIds(Index component_id, int localGeometryEdgeId);

    Q_INVOKABLE QString getModelName(Index model_id) const;

    /**
     * @brief 获取指定组件的显示名称，保持和对象树中的组件名称一致
     * @param component_id 组件 ID
     */
    Q_INVOKABLE QString getComponentName(Index component_id) const;
    /**
     * @brief 获取模型的属性名列表
     * @param model_id
     */
    Q_INVOKABLE QStringList getModelAttriName(Index model_id) const;
    /**
     * @brief 获取模型的属性类型列表
     * 现在有bug返回到控制台ui那边似乎拿不到
     * @param model_id
     */
    Q_INVOKABLE QList<Element::Type> getModelAttriType(Index model_id) const;

    /**
     * @brief 获取指定组件的属性渲染条目，供 QML 直接展示属性名、属性类型和分量数
     * @param component_id 当前选中的组件 ID
     */
    Q_INVOKABLE QVariantList getComponentAttriInfo(Index component_id) const;

    Q_INVOKABLE QVariantList listModels() const;
    Q_INVOKABLE QVariantList getComponentsSummary(Index model_id) const;
    Q_INVOKABLE QVariantMap getMeshSummary(Index component_id) const;
    Q_INVOKABLE QVariantMap getGeometrySummary(Index component_id) const;

    std::optional<GeomFaceId> resolveGeometryFaceLocalId(Index component_id, int localFaceId);
    std::optional<GeomEdgeId> resolveGeometryEdgeLocalId(Index component_id, int localEdgeId);
    std::optional<GeomVertexId> resolveGeometryVertexLocalId(Index component_id, int localVertexId);
    std::optional<GeomSolidId> resolveGeometrySolidLocalId(Index component_id, int localSolidId);

private:
    ModelLayer* m_manager;
};
