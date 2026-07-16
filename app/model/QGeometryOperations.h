#pragma once

#include "Core.h"
#include "QSelection.h"

#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>
#include <string>

class ModelLayer;
class TopoDS_Shape;

/**
 * @brief 几何操作的 QML 适配器，负责几何创建流程以及写入目标的选择。
 *
 * 该类调用 GeometryBuilder 构造 OCC Shape，再根据 Model/Component 目标
 * 将结果新建或追加到 ModelLayer，不参与底层几何算法和渲染。
 */
class QGeometryOperations : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("QGeometryOperations is provided by QModelManager")

public:
    explicit QGeometryOperations(ModelLayer& model_layer, QObject* parent = nullptr);

    /**
     * @brief 创建独立几何点，并根据当前 Model/Component 选择确定写入位置。
     *
     * @return 新建或更新的组件 ID，失败时返回 -1。
     */
    Q_INVOKABLE int createPoint(
        int modelId, int componentId, double x, double y, double z);

    /**
     * @brief 根据两个坐标点创建直线边，并按当前 Model/Component 目标写入。
     *
     * @return 新建或更新的组件 ID，失败时返回 -1。
     */
    Q_INVOKABLE int createLineByCoordinates(
        int modelId,
        int componentId,
        double startX,
        double startY,
        double startZ,
        double endX,
        double endY,
        double endZ);

    /**
     * @brief 使用当前组件中选中的两个 Geometry Vertex 创建共享拓扑的直线边。
     *
     * @return 被更新的组件 ID，失败时返回 -1。
     */
    Q_INVOKABLE int createLineFromVertices(
        int componentId, QSelection* selection);

    /**
     * @brief 创建长方体，并根据当前 Model/Component 选择确定写入位置。
     *
     * @return 新建或更新的组件 ID，失败时返回 -1。
     */
    Q_INVOKABLE int createBox(
        int modelId,
        int componentId,
        double originX,
        double originY,
        double originZ,
        double lengthX,
        double lengthY,
        double lengthZ);

signals:
    void operationFailed(const QString& message);

private:
    /**
     * @brief 根据目标 Model/Component 将 OCC Shape 新建或追加到模型层。
     */
    Index addGeometryShape(
        Index model_id,
        Index component_id,
        std::string component_name,
        TopoDS_Shape shape);

    ModelLayer* model_layer_ {};
    int next_point_number_ { 1 };
    int next_line_number_ { 1 };
    int next_box_number_ { 1 };
};
