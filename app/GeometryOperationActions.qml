/**
 * @file GeometryOperationActions.qml
 * @brief 集中管理基础几何操作的参数定义、启动状态和后端调用。
 */

import QtQuick

import app.core
import app.model

QtObject {
    id: root

    signal operationActivated()

    readonly property var createPointInfo: ({
        name: "create_point",
        display_name: qsTr("创建点"),
        description: qsTr("根据三维坐标创建独立几何点"),
        arg_types: [
            { type: QArgType.Float, name: qsTr("X 坐标"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("Y 坐标"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("Z 坐标"), content: "0", description: "" }
        ]
    })

    readonly property var createBoxInfo: ({
        name: "create_box",
        display_name: qsTr("创建长方体"),
        description: qsTr("根据原点和三个轴向尺寸创建长方体"),
        arg_types: [
            { type: QArgType.Float, name: qsTr("原点 X"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("原点 Y"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("原点 Z"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("X 方向长度"), content: "10", description: qsTr("必须大于 0") },
            { type: QArgType.Float, name: qsTr("Y 方向长度"), content: "10", description: qsTr("必须大于 0") },
            { type: QArgType.Float, name: qsTr("Z 方向长度"), content: "10", description: qsTr("必须大于 0") }
        ]
    })

    readonly property var createCylinderInfo: ({
        name: "create_cylinder",
        display_name: qsTr("创建圆柱体"),
        description: qsTr("根据底面圆心、半径、高度、轴向和扫掠角创建圆柱体"),
        arg_types: [
            { type: QArgType.Float, name: qsTr("底面圆心 X"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("底面圆心 Y"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("底面圆心 Z"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("半径"), content: "10", description: qsTr("必须大于几何容差") },
            { type: QArgType.Float, name: qsTr("高度"), content: "20", description: qsTr("必须大于几何容差") },
            { type: QArgType.Float, name: qsTr("轴向 X"), content: "0", description: qsTr("轴向不能为零向量") },
            { type: QArgType.Float, name: qsTr("轴向 Y"), content: "0", description: qsTr("轴向不能为零向量") },
            { type: QArgType.Float, name: qsTr("轴向 Z"), content: "1", description: qsTr("轴向不能为零向量") },
            { type: QArgType.Float, name: qsTr("扫掠角（度）"), content: "360", description: qsTr("范围为 (0, 360]") }
        ]
    })

    readonly property var createLineByCoordinatesInfo: ({
        name: "create_line_by_coordinates",
        display_name: qsTr("创建直线边（坐标）"),
        description: qsTr("根据起点和终点坐标创建直线边"),
        arg_types: [
            { type: QArgType.Float, name: qsTr("起点 X"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("起点 Y"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("起点 Z"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("终点 X"), content: "10", description: "" },
            { type: QArgType.Float, name: qsTr("终点 Y"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("终点 Z"), content: "0", description: "" }
        ]
    })

    readonly property var createLineFromVerticesInfo: ({
        name: "create_line_from_vertices",
        display_name: qsTr("创建直线边（选择两点）"),
        description: qsTr("选择当前组件中的两个已有几何点创建共享拓扑的直线边"),
        arg_types: [
            { type: QArgType.Selector, name: qsTr("端点"), content: "", description: qsTr("请选择两个几何点") }
        ]
    })

    readonly property var createRectangleFaceInfo: ({
        name: "create_rectangle_face",
        display_name: qsTr("创建矩形面"),
        description: qsTr("根据角点、宽度、高度和坐标平面创建矩形面"),
        arg_types: [
            { type: QArgType.Float, name: qsTr("原点 X"), content: "0", description: qsTr("矩形的一个角点") },
            { type: QArgType.Float, name: qsTr("原点 Y"), content: "0", description: qsTr("矩形的一个角点") },
            { type: QArgType.Float, name: qsTr("原点 Z"), content: "0", description: qsTr("矩形的一个角点") },
            { type: QArgType.Float, name: qsTr("宽度"), content: "10", description: qsTr("沿平面第一个坐标轴的长度") },
            { type: QArgType.Float, name: qsTr("高度"), content: "10", description: qsTr("沿平面第二个坐标轴的长度") },
            { type: QArgType.Combo, name: qsTr("平面"), content: "XY,YZ,XZ|0", description: qsTr("矩形所在的全局坐标平面") }
        ]
    })

    readonly property var createDiskFaceInfo: ({
        name: "create_disk_face",
        display_name: qsTr("创建圆盘/扇形面"),
        description: qsTr("根据圆心、半径、坐标平面和角度创建圆盘或扇形面"),
        arg_types: [
            { type: QArgType.Float, name: qsTr("圆心 X"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("圆心 Y"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("圆心 Z"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("半径"), content: "10", description: qsTr("必须大于几何容差") },
            { type: QArgType.Combo, name: qsTr("平面"), content: "XY,YZ,XZ|0", description: qsTr("圆面所在的全局坐标平面") },
            { type: QArgType.Float, name: qsTr("起始角（度）"), content: "0", description: qsTr("完整圆盘时忽略") },
            { type: QArgType.Float, name: qsTr("扫掠角（度）"), content: "360", description: qsTr("范围为 (0, 360]") }
        ]
    })

    readonly property var createFaceFromEdgesInfo: ({
        name: "create_face_from_edges",
        display_name: qsTr("选择闭合边创建面"),
        description: qsTr("选择当前组件中组成单一闭合轮廓的几何边创建平面或填充曲面"),
        arg_types: [
            { type: QArgType.Selector, name: qsTr("轮廓边"), content: "", description: qsTr("请选择一条或多条闭合轮廓边") }
        ]
    })

    readonly property var extrudeFaceInfo: ({
        name: "extrude_face",
        display_name: qsTr("拉伸面为实体"),
        description: qsTr("选择一个几何面，沿指定方向和长度拉伸为实体；源面保留"),
        arg_types: [
            { type: QArgType.Selector, name: qsTr("截面"), content: "", description: qsTr("请选择一个几何面") },
            { type: QArgType.Float, name: qsTr("方向 X"), content: "0", description: qsTr("方向不能为零向量") },
            { type: QArgType.Float, name: qsTr("方向 Y"), content: "0", description: qsTr("方向不能为零向量") },
            { type: QArgType.Float, name: qsTr("方向 Z"), content: "1", description: qsTr("方向不能为零向量") },
            { type: QArgType.Float, name: qsTr("长度"), content: "10", description: qsTr("必须大于几何容差") }
        ]
    })

    // 启动参数侧栏操作，并在需要拾取时设置选择模式和参数下标。
    function activate(operation, selectMode, selectorIndex) {
        App.activeOperation = operation
        if (selectMode)
            App.selection.selectMode = selectMode
        if (selectorIndex >= 0)
            App.selection.listeningSelectorIndex = selectorIndex
        root.operationActivated()
    }

    // 创建成功后统一更新当前 Model 和 Component，并按需清理渲染选择。
    function selectCreatedComponent(componentId, clearRenderSelection) {
        if (componentId < 0)
            return
        if (clearRenderSelection && App.registry.renderWindow)
            App.registry.renderWindow.clearSelection()
        App.selection.activeComponentId = componentId
        App.selection.activeModelId = QModelManager.query.findModelIdByComponent(componentId)
    }

    // 启动坐标创建点操作。
    function startCreatePoint() {
        activate({
            info: root.createPointInfo,
            allowWithoutModel: true,
            showGeometryTarget: true,
            defaultParameters: [0, 0, 0],
            execute: function(modelId, args) {
                root.selectCreatedComponent(QModelManager.geometry.createPoint(
                    modelId, App.selection.activeComponentId,
                    args[0], args[1], args[2]), false)
            }
        }, "", -1)
    }

    // 启动坐标创建直线边操作。
    function startCreateLineByCoordinates() {
        activate({
            info: root.createLineByCoordinatesInfo,
            allowWithoutModel: true,
            showGeometryTarget: true,
            defaultParameters: [0, 0, 0, 10, 0, 0],
            execute: function(modelId, args) {
                root.selectCreatedComponent(QModelManager.geometry.createLineByCoordinates(
                    modelId, App.selection.activeComponentId,
                    args[0], args[1], args[2],
                    args[3], args[4], args[5]), false)
            }
        }, "", -1)
    }

    // 启动选择两个已有点创建直线边操作。
    function startCreateLineFromVertices() {
        activate({
            info: root.createLineFromVerticesInfo,
            requireComponent: true,
            showGeometryTarget: true,
            defaultParameters: [null],
            execute: function(modelId, args) {
                root.selectCreatedComponent(QModelManager.geometry.createLineFromVertices(
                    App.selection.activeComponentId, args[0]), true)
            }
        }, "GeometryVertex", 0)
    }

    // 启动矩形面创建操作。
    function startCreateRectangleFace() {
        activate({
            info: root.createRectangleFaceInfo,
            allowWithoutModel: true,
            showGeometryTarget: true,
            defaultParameters: [0, 0, 0, 10, 10, 0],
            execute: function(modelId, args) {
                root.selectCreatedComponent(QModelManager.geometry.createRectangleFace(
                    modelId, App.selection.activeComponentId,
                    args[0], args[1], args[2],
                    args[3], args[4], args[5]), false)
            }
        }, "", -1)
    }

    // 启动圆盘或扇形面创建操作。
    function startCreateDiskFace() {
        activate({
            info: root.createDiskFaceInfo,
            allowWithoutModel: true,
            showGeometryTarget: true,
            defaultParameters: [0, 0, 0, 10, 0, 0, 360],
            execute: function(modelId, args) {
                root.selectCreatedComponent(QModelManager.geometry.createDiskFace(
                    modelId, App.selection.activeComponentId,
                    args[0], args[1], args[2], args[3],
                    args[4], args[5], args[6]), false)
            }
        }, "", -1)
    }

    // 启动选择闭合几何边创建平面操作。
    function startCreateFaceFromEdges() {
        activate({
            info: root.createFaceFromEdgesInfo,
            requireComponent: true,
            showGeometryTarget: true,
            defaultParameters: [null],
            execute: function(modelId, args) {
                root.selectCreatedComponent(QModelManager.geometry.createFaceFromEdges(
                    App.selection.activeComponentId, args[0]), true)
            }
        }, "GeometryEdge", 0)
    }

    // 启动长方体创建操作。
    function startCreateBox() {
        activate({
            info: root.createBoxInfo,
            allowWithoutModel: true,
            showGeometryTarget: true,
            defaultParameters: [0, 0, 0, 10, 10, 10],
            execute: function(modelId, args) {
                root.selectCreatedComponent(QModelManager.geometry.createBox(
                    modelId, App.selection.activeComponentId,
                    args[0], args[1], args[2],
                    args[3], args[4], args[5]), false)
            }
        }, "", -1)
    }

    // 启动圆柱体创建操作。
    function startCreateCylinder() {
        activate({
            info: root.createCylinderInfo,
            allowWithoutModel: true,
            showGeometryTarget: true,
            defaultParameters: [0, 0, 0, 10, 20, 0, 0, 1, 360],
            execute: function(modelId, args) {
                root.selectCreatedComponent(QModelManager.geometry.createCylinder(
                    modelId, App.selection.activeComponentId,
                    args[0], args[1], args[2],
                    args[3], args[4], args[5], args[6], args[7], args[8]), false)
            }
        }, "", -1)
    }

    // 启动选择面拉伸为实体操作。
    function startExtrudeFace() {
        activate({
            info: root.extrudeFaceInfo,
            requireComponent: true,
            showGeometryTarget: true,
            defaultParameters: [null, 0, 0, 1, 10],
            execute: function(modelId, args) {
                root.selectCreatedComponent(QModelManager.geometry.extrudeFace(
                    App.selection.activeComponentId,
                    args[0], args[1], args[2], args[3], args[4]), true)
            }
        }, "GeometryFace", 0)
    }
}
