#pragma once

#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <atomic>
#include <exception>
#include <memory>
#include <vector>

/**
 * @brief 一组在 OCC 数值精度内覆盖相同几何区域的面。
 */
struct GeometryDuplicateFaceGroup {
    std::vector<TopoDS_Face> faces;
};

//! @brief 一对互相穿插的面，自相交与几何干涉共用；单 Face 自交时两个成员相同。
struct GeometryIntersectingFacePair {
    TopoDS_Face first;
    TopoDS_Face second;
};

/**
 * @brief 几何拓扑诊断的完整结果。
 */
struct GeometryTopologyDiagnosticResult {
    std::vector<TopoDS_Edge> boundary_edges;
    std::vector<TopoDS_Edge> isolated_edges;
    std::vector<TopoDS_Edge> non_manifold_edges;
    std::vector<TopoDS_Edge> small_edges;
    std::vector<TopoDS_Face> small_faces;
    std::vector<GeometryDuplicateFaceGroup> duplicate_face_groups;
    //! 自相交：单 Face 自交，或同一 Solid 内互相穿插的面对。
    std::vector<GeometryIntersectingFacePair> self_intersecting_face_pairs;
    //! 几何干涉：不同 Solid 或自由 Surface 之间相交的面对。
    //! 与前处理器一致：重叠（含共面/贴合）与穿越都算。
    std::vector<GeometryIntersectingFacePair> interfering_face_pairs;
    std::vector<TopoDS_Shape> invalid_shapes;
};

/**
 * @brief 协作式取消信号。
 *
 * 只在候选对之间与各阶段边界检查标记，**不强制中断**任何单次 OCC 运算，
 * 因此取消延迟最坏等于最慢的一对 Face。调用方据此丢弃结果即可。
 */
class GeometryTopologyDiagnosticCancelled : public std::exception {
public:
    explicit GeometryTopologyDiagnosticCancelled(const char* message) noexcept
        : message_(message)
    {
    }

    const char* what() const noexcept override { return message_; }

private:
    const char* message_;
};

/**
 * @brief 控制一次诊断需要计算的类别，避免只看边时执行昂贵的面两两求交。
 */
struct GeometryTopologyDiagnosticOptions {
    bool edge_topology { true };
    bool small_edges { true };
    bool small_faces { true };
    bool duplicate_faces { true };
    //! 自相交：检查单 Surface 自交及同一 Solid 内部的 Surface 相交。
    bool self_intersecting_faces { true };
    //! 几何干涉：检查不同 Solid 或自由 Surface 之间的相交；只要面相交就算
    //! （重叠与穿越都算，与前处理器一致）。
    bool interfering_faces { true };
    bool invalid_topology { true };
};

/**
 * @brief 可分段推进的一次几何拓扑诊断。
 *
 * 判定顺序、判定谓词与 GeometryTopologyEditor::diagnoseTopology() 完全相同，区别只在于
 * 工作可以按时间片分批完成：调用方（后台任务队列）因此能在时间片之间让更紧急的请求插队，
 * 不必等一次长诊断整体跑完。结果只有在 advance() 返回 true 之后才是完整的。
 */
class GeometryTopologyDiagnosticSession {
public:
    /**
     * @brief 开始一次诊断。参数校验失败时抛 std::invalid_argument，与 diagnoseTopology() 一致。
     */
    static std::unique_ptr<GeometryTopologyDiagnosticSession> start(
        const TopoDS_Shape& root,
        double small_edge_length_threshold,
        double small_face_area_threshold,
        const GeometryTopologyDiagnosticOptions& options,
        const std::atomic<bool>* cancel = nullptr);

    virtual ~GeometryTopologyDiagnosticSession() = default;

    /**
     * @brief 最多推进 budget_ms 毫秒。
     * @param budget_ms 时间预算；非有限值表示一直推进到完成。
     * @return 全部工作是否已完成。
     */
    virtual bool advance(double budget_ms) = 0;

    //! 取走诊断结果；只应在 advance() 返回 true 之后调用。
    virtual GeometryTopologyDiagnosticResult takeResult() = 0;
};

/**
 * @brief 提供不依赖界面和模型管理的基础 OCC 拓扑编辑函数。
 */
class GeometryTopologyEditor {
public:
    /**
     * @brief 计算用于展示和后续清理的几何拓扑诊断结果。
     *
     * 边按相邻面的数量分为孤立边、边界边和非流形边；尺寸诊断按独立的长度、
     * 面积阈值筛选细小边和细小面；重复面、自相交、几何干涉和无效拓扑使用 OCC 数值精度。
     * 自相交检查单 Face 及同一真实 Solid 内部；几何干涉独立检查不同 Solid/自由 Surface，
     * 不根据包围盒推测 Part；不同 Solid 或自由 Surface 之间的面相交即报。
     *
     * @param root 要诊断的几何根形状。
     * @param small_edge_length_threshold 细小边长度阈值，必须大于零。
     * @param small_face_area_threshold 细小面面积阈值，必须大于零。
     * @param options 本次需要计算的诊断类别。
     * @param cancel 可选的协作式取消标记；置位后在候选对之间抛出
     *        GeometryTopologyDiagnosticCancelled，不会中断单次 OCC 运算。
     * @return 一次计算得到的全部诊断类别。
     *
     * 等价于用无限预算把 GeometryTopologyDiagnosticSession 一次推进到完成；需要让长诊断
     * 给其它请求让路时，改用会话接口分片推进。
     */
    static GeometryTopologyDiagnosticResult diagnoseTopology(
        const TopoDS_Shape& root,
        double small_edge_length_threshold,
        double small_face_area_threshold,
        const GeometryTopologyDiagnosticOptions& options = {},
        const std::atomic<bool>* cancel = nullptr);

    /**
     * @brief 从根形状中删除一个顶层独立 Vertex、Edge、Face 或 Solid。
     *
     * @param root 当前几何根形状。
     * @param target 要删除的形状，必须是根形状本身或根 Compound 的直接子形状。
     * @param delete_children 是否同时删除目标的独占下级拓扑；为 false 时将目标的直接
     * 下级拓扑提升为根 Compound 的直接子形状。
     * @return 删除后的根形状；没有任何剩余拓扑时返回空 Shape。
     *
     * @throws std::invalid_argument 输入为空、类型不支持或目标不是顶层独立形状。
     * @throws std::runtime_error 编辑后的拓扑无效。
     */
    static TopoDS_Shape removeTopLevelShape(
        const TopoDS_Shape& root,
        const TopoDS_Shape& target,
        bool delete_children);
};
