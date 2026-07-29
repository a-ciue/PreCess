#pragma once

#include <TopoDS_Shape.hxx>
#include <vector>

class TopoDS_Edge;
class TopoDS_Face;
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
     * @brief 根据圆心、半径、坐标平面和角度创建圆盘或扇形面。
     *
     * 角度单位为弧度；完整圆盘忽略起始角，局部扇形从起始角沿正方向扫掠。
     * @throws std::invalid_argument 输入包含非有限数值、半径过小、角度越界或平面无效。
     * @throws std::runtime_error OCC 构造失败或结果拓扑无效。
     */
    static TopoDS_Shape makeDiskFace(
        double center_x,
        double center_y,
        double center_z,
        double radius,
        CoordinatePlane plane,
        double start_angle,
        double sweep_angle);

    /**
     * @brief 使用一组已有几何边构造单一闭合 Face。
     *
     * 输入边可以不按连接顺序排列；共面轮廓创建精确平面，非共面轮廓创建 C0 填充曲面。
     * @throws std::invalid_argument 边集合为空、包含空边或不能组成闭合轮廓。
     * @throws std::runtime_error OCC 构造 Wire/Face 失败或结果拓扑无效。
     */
    static TopoDS_Shape makeFaceFromEdges(
        const std::vector<TopoDS_Edge>& edges);

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

    /**
     * @brief 根据底面圆心、半径、高度、轴向和扫掠角创建圆柱体。
     *
     * 扫掠角单位为弧度；完整角度创建整圆柱，其余角度创建封闭的部分圆柱。
     * @throws std::invalid_argument 输入包含非有限数值、尺寸过小、轴向为零或角度越界。
     * @throws std::runtime_error OCC 构造失败或结果拓扑无效。
     */
    static TopoDS_Shape makeCylinder(
        double center_x,
        double center_y,
        double center_z,
        double radius,
        double height,
        double direction_x,
        double direction_y,
        double direction_z,
        double sweep_angle);

    /**
     * @brief 根据底面圆心、底/顶半径、高度、轴向和扫掠角创建圆锥或圆台。
     *
     * 半径允许一个为零以创建尖圆锥；扫掠角单位为弧度，完整角度创建完整体，其余角度创建封闭的部分体。
     * @throws std::invalid_argument 输入包含非有限数值、尺寸无效、轴向为零或角度越界。
     * @throws std::runtime_error OCC 构造失败或结果拓扑无效。
     */
    static TopoDS_Shape makeCone(
        double center_x,
        double center_y,
        double center_z,
        double bottom_radius,
        double top_radius,
        double height,
        double direction_x,
        double direction_y,
        double direction_z,
        double sweep_angle);

    /**
     * @brief 根据球心、半径、轴向、纬度范围和经度扫掠角创建球体或部分球体。
     *
     * 角度单位为弧度；纬度范围为 [-PI/2, PI/2]，经度扫掠范围为 (0, 2*PI]。
     * @throws std::invalid_argument 输入包含非有限数值、尺寸无效、轴向为零或角度越界。
     * @throws std::runtime_error OCC 构造失败或结果拓扑无效。
     */
    static TopoDS_Shape makeSphere(
        double center_x,
        double center_y,
        double center_z,
        double radius,
        double direction_x,
        double direction_y,
        double direction_z,
        double minimum_latitude,
        double maximum_latitude,
        double longitude_sweep);

    /**
     * @brief 将已有面沿指定方向和长度拉伸为实体，构造时复制源面。
     *
     * @throws std::invalid_argument 源面为空、输入包含非有限数值、长度过小或方向为零。
     * @throws std::runtime_error OCC 构造失败、未生成实体或结果拓扑无效。
     */
    static TopoDS_Shape extrudeFace(
        const TopoDS_Face& face,
        double direction_x,
        double direction_y,
        double direction_z,
        double length);
};
