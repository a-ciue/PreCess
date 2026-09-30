/**
 * @file GeometryRepairValidation.h
 * @brief 几何编辑共用的局部精度、曲线偏差预检和边界验收；不包含搜索距离或用户移动意图。
 */
#pragma once

#include <TopoDS_Edge.hxx>
#include <TopoDS_Vertex.hxx>
#include <functional>
#include <vector>
#include <utility>

class TopoDS_Face;
class gp_Pnt;

namespace geometry::repair {
/** @brief 在模型当前坐标单位下计算的局部精度，搜索距离不得参与计算。 */
struct PrecisionPolicy {
    double numerical_floor = 0.0; //!< 内核与坐标舍入共同决定的精度底线。
    double fitting = 0.0; //!< 局部尺度与最短边限制下的求解目标。
    double validation = 0.0; //!< 本次新建几何允许的表示误差上限。
    double classification = 0.0; //!< 判断曲线是否在面上的数值精度。

    /** @brief 从操作局部形状推导精度，不能传整个无关装配来放宽小零件的精度。 */
    static PrecisionPolicy fromShape(const TopoDS_Shape& local_shape);
};

/** @brief 自适应预检结果；预算不足或投影失败必须与合格结果区分。 */
enum class DeviationStatus { WithinLimit,
    Exceeded,
    Unresolved };

/** @brief 记录观测最大偏差与求值次数；自适应预检不代替最终 pcurve 验收。 */
struct DeviationResult {
    DeviationStatus status = DeviationStatus::Unresolved;
    double maximum = 0.0;
    int evaluations = 0;
};

/** @brief 预检计算预算；深度和求值上限只限制成本，不允许在耗尽时返回成功。 */
struct DeviationBudget {
    int max_depth = 16;
    int max_evaluations = 16384;
};

/**
 * @brief 按曲线节点区间、弦偏差和距离变化递归细分，检查三维曲线到目标的距离。
 * @param distance 返回给定点的目标距离；负值或非有限值表示无法求解。
 */
DeviationResult measureCurveDeviation(const TopoDS_Edge& edge,
    const std::function<double(const gp_Pnt&)>& distance, double limit,
    const DeviationBudget& budget = { });

/**
 * @brief 修复前捕获边、点身份及各自误差上限；修复后检查拓扑及三维/二维曲线一致性。
 * 既有容差仅作用于对应实体，不传播成整个局部区域的求解容差。
 */
class BoundaryAudit {
public:
    /** @brief 在任何修复器修改边界之前捕获身份和原始容差。 */
    BoundaryAudit(const TopoDS_Shape& boundary, const PrecisionPolicy& precision);
    /** @brief 要求边界身份及容差受控，并使用 OCC 最大偏差检查验证面上的每条边。 */
    void validateFace(const TopoDS_Face& face, const char* operation) const;

private:
    std::vector<std::pair<TopoDS_Edge, double>> edges_;
    std::vector<std::pair<TopoDS_Vertex, double>> vertices_;
};
}
