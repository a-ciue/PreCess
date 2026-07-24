/**
 * @file InteractiveTypes.h
 * @brief 视口交互的纯数据类型：拾取结果与标注图元（无 VTK 依赖，供渲染层与插件两侧共用）
 */
#ifndef INTERACTIVE_TYPES_H
#define INTERACTIVE_TYPES_H

#include "Core.h"

#include <array>
#include <string>
#include <vector>

namespace systems::interaction {

//! @brief 拾取结果：渲染层解析好的吸附点（世界坐标 + 两套全局顶点 id）
struct PickInfo {
    std::array<double, 3> world_pos {};
    Index mesh_id = -1; //> 网格全局顶点 id（vtkOriginalPointIds），非网格点为 -1
    Index geom_id = -1; //> 几何全局顶点 id（GeometryRegistry id），非几何点为 -1
    bool valid = false; //> 是否命中吸附点（悬停未命中时为 false，供 handler 清预览）
};

//! @brief 标注点（颜色分量 0~1，默认红色）
struct AnnotationPoint {
    std::array<double, 3> pos;
    double r = 1, g = 0, b = 0;
};

//! @brief 标注线段（默认绿色实线；dashed 用于悬停动态预览）
struct AnnotationLine {
    std::array<double, 3> p0, p1;
    double r = 0.2, g = 0.85, b = 0.2;
    bool dashed = false;
};

//! @brief 标注文本（默认白色）
struct AnnotationText {
    std::array<double, 3> pos;
    std::string text;
    double r = 1, g = 1, b = 1;
};

//! @brief 一帧标注集：插件按当前状态回传，渲染层统一绘制
struct AnnotationBatch {
    std::vector<AnnotationPoint> points;
    std::vector<AnnotationLine> lines;
    std::vector<AnnotationText> texts;

    void clear()
    {
        points.clear();
        lines.clear();
        texts.clear();
    }
};

} // namespace systems::interaction

#endif // INTERACTIVE_TYPES_H
