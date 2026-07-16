#pragma once

#include <TopoDS_Shape.hxx>

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
