/**
 * @file GeometryOperationActions.qml
 * @brief 集中管理基础几何创建和编辑操作的参数定义、启动状态和后端调用。
 */

import QtQuick

import app.core
import app.model

QtObject {
    id: root

    signal operationActivated()

    // “写入目标”下拉框的固定索引，需与参数选项顺序保持一致。
    readonly property int appendToCurrentComponent: 0
    readonly property int createNewComponent: 1
    readonly property int createNewModel: 2

    readonly property var createPointInfo: ({
        name: "create_point",
        display_name: qsTr("创建点"),
        description: qsTr("根据三维坐标创建独立几何点"),
        arg_types: [
            { type: QArgType.Float, name: qsTr("X 坐标"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("Y 坐标"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("Z 坐标"), content: "0", description: "" },
            { type: QArgType.Combo, name: qsTr("写入目标"), content: "添加到当前 Component,新建 Component,新建 Model|0", description: qsTr("选择几何创建结果的组织位置") }
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
            { type: QArgType.Float, name: qsTr("Z 方向长度"), content: "10", description: qsTr("必须大于 0") },
            { type: QArgType.Combo, name: qsTr("写入目标"), content: "添加到当前 Component,新建 Component,新建 Model|0", description: qsTr("选择几何创建结果的组织位置") }
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
            { type: QArgType.Float, name: qsTr("扫掠角（度）"), content: "360", description: qsTr("范围为 (0, 360]") },
            { type: QArgType.Combo, name: qsTr("写入目标"), content: "添加到当前 Component,新建 Component,新建 Model|0", description: qsTr("选择几何创建结果的组织位置") }
        ]
    })

    readonly property var createConeInfo: ({
        name: "create_cone",
        display_name: qsTr("创建圆锥/圆台"),
        description: qsTr("根据底面圆心、底/顶半径、高度、轴向和扫掠角创建圆锥或圆台"),
        arg_types: [
            { type: QArgType.Float, name: qsTr("底面圆心 X"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("底面圆心 Y"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("底面圆心 Z"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("底面半径"), content: "10", description: qsTr("必须大于等于 0，且与顶面半径不同") },
            { type: QArgType.Float, name: qsTr("顶面半径"), content: "5", description: qsTr("必须大于等于 0；设为 0 可创建尖圆锥") },
            { type: QArgType.Float, name: qsTr("高度"), content: "20", description: qsTr("必须大于几何容差") },
            { type: QArgType.Float, name: qsTr("轴向 X"), content: "0", description: qsTr("轴向不能为零向量") },
            { type: QArgType.Float, name: qsTr("轴向 Y"), content: "0", description: qsTr("轴向不能为零向量") },
            { type: QArgType.Float, name: qsTr("轴向 Z"), content: "1", description: qsTr("轴向不能为零向量") },
            { type: QArgType.Float, name: qsTr("扫掠角（度）"), content: "360", description: qsTr("范围为 (0, 360]") },
            { type: QArgType.Combo, name: qsTr("写入目标"), content: "添加到当前 Component,新建 Component,新建 Model|0", description: qsTr("选择几何创建结果的组织位置") }
        ]
    })

    readonly property var createSphereInfo: ({
        name: "create_sphere",
        display_name: qsTr("创建球体/部分球体"),
        description: qsTr("根据球心、半径、轴向、纬度范围和经度扫掠角创建球体"),
        arg_types: [
            { type: QArgType.Float, name: qsTr("球心 X"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("球心 Y"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("球心 Z"), content: "0", description: "" },
            { type: QArgType.Float, name: qsTr("半径"), content: "10", description: qsTr("必须大于几何容差") },
            { type: QArgType.Float, name: qsTr("极轴 X"), content: "0", description: qsTr("极轴不能为零向量") },
            { type: QArgType.Float, name: qsTr("极轴 Y"), content: "0", description: qsTr("极轴不能为零向量") },
            { type: QArgType.Float, name: qsTr("极轴 Z"), content: "1", description: qsTr("极轴不能为零向量") },
            { type: QArgType.Float, name: qsTr("最小纬度（度）"), content: "-90", description: qsTr("范围为 [-90, 90)，且小于最大纬度") },
            { type: QArgType.Float, name: qsTr("最大纬度（度）"), content: "90", description: qsTr("范围为 (-90, 90]，且大于最小纬度") },
            { type: QArgType.Float, name: qsTr("经度扫掠角（度）"), content: "360", description: qsTr("范围为 (0, 360]") },
            { type: QArgType.Combo, name: qsTr("写入目标"), content: "添加到当前 Component,新建 Component,新建 Model|0", description: qsTr("选择几何创建结果的组织位置") }
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
            { type: QArgType.Float, name: qsTr("终点 Z"), content: "0", description: "" },
            { type: QArgType.Combo, name: qsTr("写入目标"), content: "添加到当前 Component,新建 Component,新建 Model|0", description: qsTr("选择几何创建结果的组织位置") }
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
            { type: QArgType.Combo, name: qsTr("平面"), content: "XY,YZ,XZ|0", description: qsTr("矩形所在的全局坐标平面") },
            { type: QArgType.Combo, name: qsTr("写入目标"), content: "添加到当前 Component,新建 Component,新建 Model|0", description: qsTr("选择几何创建结果的组织位置") }
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
            { type: QArgType.Float, name: qsTr("扫掠角（度）"), content: "360", description: qsTr("范围为 (0, 360]") },
            { type: QArgType.Combo, name: qsTr("写入目标"), content: "添加到当前 Component,新建 Component,新建 Model|0", description: qsTr("选择几何创建结果的组织位置") }
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

    readonly property var deleteGeometryInfo: ({
        name: "delete_geometry",
        display_name: qsTr("删除几何"),
        description: qsTr("删除一个顶层独立几何点、边、面或体，可选择是否同时删除其独占下级拓扑"),
        arg_types: [
            { type: QArgType.Selector, name: qsTr("目标几何"), content: "", description: qsTr("请在顶部选择器中切换几何点、边、面或体模式") },
            { type: QArgType.Bool, name: qsTr("同时删除下级拓扑"), content: "false", description: qsTr("关闭时保留直接下级拓扑，开启时不影响其他形状共享的拓扑") }
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

    // 根据当前业务选择计算独立几何操作的默认写入目标。
    function defaultGeometryTarget() {
        if (App.selection.activeComponentId >= 0)
            return root.appendToCurrentComponent
        if (App.selection.activeModelId >= 0)
            return root.createNewComponent
        return root.createNewModel
    }

    // 根据下拉框选择解析 Model/Component 目标，再调用具体几何创建函数。
    function createIndependentGeometry(componentId, args, targetIndex, createShape) {
        const writeTarget = args[targetIndex]
        let targetModelId = App.selection.activeModelId
        let targetComponentId = componentId

        if (writeTarget === root.appendToCurrentComponent) {
            if (targetComponentId < 0)
                return qsTr("请选择当前 Component，或修改写入目标。")
        } else if (writeTarget === root.createNewComponent) {
            if (targetModelId < 0)
                return qsTr("请选择当前 Model，或将写入目标改为“新建 Model”。")
            targetComponentId = -1
        } else if (writeTarget === root.createNewModel) {
            targetModelId = -1
            targetComponentId = -1
        } else {
            return qsTr("写入目标参数无效。")
        }

        return root.finishGeometryOperation(
            createShape(targetModelId, targetComponentId), false)
    }

    // 依赖已有拓扑的操作必须写回其来源 Component。
    function requireComponent(componentId) {
        return componentId >= 0 ? "" : qsTr("请先选择目标 Component。")
    }

    // 统一处理几何操作结果；失败详情继续由 C++ 写入日志。
    function finishGeometryOperation(componentId, clearRenderSelection) {
        if (componentId < 0)
            return qsTr("几何操作失败，详细原因请查看日志。")
        root.selectCreatedComponent(componentId, clearRenderSelection)
        return ""
    }

    // 创建成功后统一更新当前 Model 和 Component，清理选择并在 Actor 更新后重置视角。
    function selectCreatedComponent(componentId, clearRenderSelection) {
        if (componentId < 0)
            return
        if (clearRenderSelection && App.registry.renderWindow)
            App.registry.renderWindow.clearSelection()
        App.selection.activeComponentId = componentId
        App.selection.activeModelId = QModelManager.query.findModelIdByComponent(componentId)
        Qt.callLater(function() {
            if (App.registry.renderWindow)
                App.registry.renderWindow.resetCamera()
        })
    }

    // 启动坐标创建点操作。
    function startCreatePoint() {
        activate({
            info: root.createPointInfo,
            defaultParameters: [0, 0, 0, root.defaultGeometryTarget()],
            execute: function(componentId, args) {
                return root.createIndependentGeometry(
                    componentId, args, 3, function(targetModelId, targetComponentId) {
                        return QModelManager.geometry.createPoint(
                            targetModelId, targetComponentId,
                            args[0], args[1], args[2])
                    })
            }
        }, "", -1)
    }

    // 启动坐标创建直线边操作。
    function startCreateLineByCoordinates() {
        activate({
            info: root.createLineByCoordinatesInfo,
            defaultParameters: [0, 0, 0, 10, 0, 0, root.defaultGeometryTarget()],
            execute: function(componentId, args) {
                return root.createIndependentGeometry(
                    componentId, args, 6, function(targetModelId, targetComponentId) {
                        return QModelManager.geometry.createLineByCoordinates(
                            targetModelId, targetComponentId,
                            args[0], args[1], args[2],
                            args[3], args[4], args[5])
                    })
            }
        }, "", -1)
    }

    // 启动选择两个已有点创建直线边操作。
    function startCreateLineFromVertices() {
        activate({
            info: root.createLineFromVerticesInfo,
            defaultParameters: [null],
            execute: function(componentId, args) {
                const error = root.requireComponent(componentId)
                if (error)
                    return error
                if (!args[0] || args[0].size() !== 2)
                    return qsTr("请选择两个几何点。")
                return root.finishGeometryOperation(
                    QModelManager.geometry.createLineFromVertices(
                        componentId, args[0]), true)
            }
        }, "GeometryVertex", 0)
    }

    // 启动矩形面创建操作。
    function startCreateRectangleFace() {
        activate({
            info: root.createRectangleFaceInfo,
            defaultParameters: [0, 0, 0, 10, 10, 0, root.defaultGeometryTarget()],
            execute: function(componentId, args) {
                return root.createIndependentGeometry(
                    componentId, args, 6, function(targetModelId, targetComponentId) {
                        return QModelManager.geometry.createRectangleFace(
                            targetModelId, targetComponentId,
                            args[0], args[1], args[2],
                            args[3], args[4], args[5])
                    })
            }
        }, "", -1)
    }

    // 启动圆盘或扇形面创建操作。
    function startCreateDiskFace() {
        activate({
            info: root.createDiskFaceInfo,
            defaultParameters: [0, 0, 0, 10, 0, 0, 360, root.defaultGeometryTarget()],
            execute: function(componentId, args) {
                return root.createIndependentGeometry(
                    componentId, args, 7, function(targetModelId, targetComponentId) {
                        return QModelManager.geometry.createDiskFace(
                            targetModelId, targetComponentId,
                            args[0], args[1], args[2], args[3],
                            args[4], args[5], args[6])
                    })
            }
        }, "", -1)
    }

    // 启动选择闭合几何边创建平面操作。
    function startCreateFaceFromEdges() {
        activate({
            info: root.createFaceFromEdgesInfo,
            defaultParameters: [null],
            execute: function(componentId, args) {
                const error = root.requireComponent(componentId)
                if (error)
                    return error
                if (!args[0] || args[0].size() < 1)
                    return qsTr("请选择闭合轮廓边。")
                return root.finishGeometryOperation(
                    QModelManager.geometry.createFaceFromEdges(
                        componentId, args[0]), true)
            }
        }, "GeometryEdge", 0)
    }

    // 启动删除顶层独立几何操作，并保留用户当前使用的几何选择模式。
    function startDeleteGeometry() {
        const geometryModes = ["GeometryVertex", "GeometryEdge",
                               "GeometryFace", "GeometrySolid"]
        const selectMode = geometryModes.indexOf(App.selection.selectMode) >= 0
                         ? App.selection.selectMode : "GeometryFace"
        activate({
            info: root.deleteGeometryInfo,
            defaultParameters: [null, false],
            execute: function(_, args) {
                const resultComponentId =
                    QModelManager.geometry.deleteGeometry(
                        args[0], args[1])
                if (resultComponentId >= 0)
                    App.selection.selectionInvalidated()
            }
        }, selectMode, 0)
    }

    // 启动长方体创建操作。
    function startCreateBox() {
        activate({
            info: root.createBoxInfo,
            defaultParameters: [0, 0, 0, 10, 10, 10, root.defaultGeometryTarget()],
            execute: function(componentId, args) {
                return root.createIndependentGeometry(
                    componentId, args, 6, function(targetModelId, targetComponentId) {
                        return QModelManager.geometry.createBox(
                            targetModelId, targetComponentId,
                            args[0], args[1], args[2],
                            args[3], args[4], args[5])
                    })
            }
        }, "", -1)
    }

    // 启动圆柱体创建操作。
    function startCreateCylinder() {
        activate({
            info: root.createCylinderInfo,
            defaultParameters: [0, 0, 0, 10, 20, 0, 0, 1, 360, root.defaultGeometryTarget()],
            execute: function(componentId, args) {
                return root.createIndependentGeometry(
                    componentId, args, 9, function(targetModelId, targetComponentId) {
                        return QModelManager.geometry.createCylinder(
                            targetModelId, targetComponentId,
                            args[0], args[1], args[2],
                            args[3], args[4], args[5], args[6], args[7], args[8])
                    })
            }
        }, "", -1)
    }

    // 启动圆锥或圆台创建操作。
    function startCreateCone() {
        activate({
            info: root.createConeInfo,
            defaultParameters: [0, 0, 0, 10, 5, 20, 0, 0, 1, 360, root.defaultGeometryTarget()],
            execute: function(componentId, args) {
                return root.createIndependentGeometry(
                    componentId, args, 10, function(targetModelId, targetComponentId) {
                        return QModelManager.geometry.createCone(
                            targetModelId, targetComponentId,
                            args[0], args[1], args[2], args[3], args[4],
                            args[5], args[6], args[7], args[8], args[9])
                    })
            }
        }, "", -1)
    }

    // 启动完整或部分球体创建操作。
    function startCreateSphere() {
        activate({
            info: root.createSphereInfo,
            defaultParameters: [0, 0, 0, 10, 0, 0, 1, -90, 90, 360, root.defaultGeometryTarget()],
            execute: function(componentId, args) {
                return root.createIndependentGeometry(
                    componentId, args, 10, function(targetModelId, targetComponentId) {
                        return QModelManager.geometry.createSphere(
                            targetModelId, targetComponentId,
                            args[0], args[1], args[2], args[3], args[4],
                            args[5], args[6], args[7], args[8], args[9])
                    })
            }
        }, "", -1)
    }

    // 启动选择面拉伸为实体操作。
    function startExtrudeFace() {
        activate({
            info: root.extrudeFaceInfo,
            defaultParameters: [null, 0, 0, 1, 10],
            execute: function(componentId, args) {
                const error = root.requireComponent(componentId)
                if (error)
                    return error
                if (!args[0] || args[0].size() !== 1)
                    return qsTr("请选择一个几何面。")
                return root.finishGeometryOperation(
                    QModelManager.geometry.extrudeFace(
                        componentId,
                        args[0], args[1], args[2], args[3], args[4]), true)
            }
        }, "GeometryFace", 0)
    }
}
