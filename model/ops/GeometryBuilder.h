#pragma once

#include <TopoDS_Shape.hxx>

class TopoDS_Vertex;

/**
 * @brief 参数创建平面几何时使用的全局坐标平面。
 */
enum class CoordinatePlane {
    XY = 0,
    YZ = 1,
    XZ = 2
};

/**
 * @brief 提供不依赖界面和模型管理的基础 OCC 几何构造函数。
 */
class GeometryBuilder {
public:
    /**
     * @brief 根据三维坐标创建独立几何点。
     *
     * @throws std::invalid_argument 输入包含非有限数值。
     * @throws std::runtime_error OCC 构造失败或结果拓扑无效。
     */
    static TopoDS_Shape makePoint(double x, double y, double z);

    /**
     * @brief 根据起点和终点坐标创建直线边。
     *
     * @throws std::invalid_argument 坐标不是有限数或两点重合。
     * @throws std::runtime_error OCC 构造失败或结果拓扑无效。
     */
    static TopoDS_Shape makeLine(
        double start_x,
        double start_y,
        double start_z,
        double end_x,
        double end_y,
        double end_z);

    /**
     * @brief 使用两个已有拓扑点创建共享端点的直线边。
     *
     * @throws std::invalid_argument 顶点为空或两点重合。
     * @throws std::runtime_error OCC 构造失败或结果拓扑无效。
     */
    static TopoDS_Shape makeLine(
        const TopoDS_Vertex& start,
        const TopoDS_Vertex& end);

    /**
     * @brief 根据角点、宽度、高度和全局坐标平面创建矩形平面。
     *
     * @throws std::invalid_argument 输入包含非有限数值、尺寸过小或平面无效。
     * @throws std::runtime_error OCC 构造失败或结果拓扑无效。
     */
    static TopoDS_Shape makeRectangleFace(
        double origin_x,
        double origin_y,
        double origin_z,
        double width,
        double height,
        CoordinatePlane plane);

    /**
     * @brief 根据圆心、半径和全局坐标平面创建圆面。
     *
     * @throws std::invalid_argument 输入包含非有限数值、半径过小或平面无效。
     * @throws std::runtime_error OCC 构造失败或结果拓扑无效。
     */
    static TopoDS_Shape makeDiskFace(
        double center_x,
        double center_y,
        double center_z,
        double radius,
        CoordinatePlane plane);

    /**
     * @brief 根据原点和三个轴向尺寸创建长方体。
     *
     * @throws std::invalid_argument 输入包含非有限数值或尺寸不大于零。
     * @throws std::runtime_error OCC 构造失败或结果拓扑无效。
     */
    static TopoDS_Shape makeBox(
        double origin_x,
        double origin_y,
        double origin_z,
        double length_x,
        double length_y,
        double length_z);
};
