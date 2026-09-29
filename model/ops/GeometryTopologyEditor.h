#pragma once

#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>

#include <atomic>
#include <exception>
#include <memory>
#include <vector>

class TopoDS_Vertex;
class gp_Pnt;

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
 * @brief 一对能够在给定清理容差内建立共享拓扑的自由边候选。
 */
struct GeometryStitchCandidate {
    TopoDS_Edge first;
    TopoDS_Edge second;
    double maximum_gap { 0.0 };
};

/**
 * @brief 自动修复自由边间隙后的结果与统计信息。
 */
struct GeometryGapRepairResult {
    TopoDS_Shape shape;
    std::size_t candidate_count { 0 };
    int stitched_edge_count { 0 };
};

/**
 * @brief 一组可与种子链缝合的对侧自由边链及其最大间隙。
 */
struct GeometryGapPartnerChain {
    std::vector<TopoDS_Edge> edges;
    double maximum_gap { 0.0 };
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
     * @brief 将一条 Edge 按归一化比例分成两条 Edge。
     * @param ratio 从起点到终点的比例，必须位于 (0, 1)。
     */
    static TopoDS_Shape splitEdge(
        const TopoDS_Shape& root,
        const TopoDS_Edge& edge,
        double ratio);

    /**
     * @brief 将一条 Edge 压缩到目标位置。
     * @param target_position 压缩后的公共顶点位置。
     */
    static TopoDS_Shape collapseEdge(
        const TopoDS_Shape& root,
        const TopoDS_Edge& edge,
        const gp_Pnt& target_position);

    /**
     * @brief 根据相邻曲线、Face、边数和边长推荐应保留的端点。
     */
    static TopoDS_Vertex recommendCollapseVertex(
        const TopoDS_Shape& root,
        const TopoDS_Edge& edge);

    /**
     * @brief 将两个或多个 Vertex 合并到目标位置。
     * @param target_position 合并后的公共顶点位置。
     */
    static TopoDS_Shape mergeVertices(
        const TopoDS_Shape& root,
        const std::vector<TopoDS_Vertex>& vertices,
        const gp_Pnt& target_position);

    /**
     * @brief 将一组连通且同域的 Face 合并为一个 Face。
     *
     * 删除选中面之间的内部共享边；若同一 Edge 还是根 Compound 的独立子节点，也会一并删除。
     * 其他边界保持不变，避免同时合并 root 中未选中的同域面。
     *
     * @param root 当前几何根形状。
     * @param faces 两个或多个属于 root 的待合并面。
     * @return 合并后的完整根形状。
     *
     * @throws std::invalid_argument 输入为空、面数量不足、存在重复面或面不属于 root。
     * @throws std::runtime_error 所选面不连通、不同域、未能完整合并或结果拓扑无效。
     */
    static TopoDS_Shape mergeFaces(
        const TopoDS_Shape& root,
        const std::vector<TopoDS_Face>& faces);

    /**
     * @brief 将一组连续且同域的 Edge 合并为一个 Edge。
     *
     * 仅开放选中边之间只连接两条选中边的公共顶点，端点及其他分支节点保持不变。
     *
     * @param root 当前几何根形状。
     * @param edges 两个或多个属于 root 的待合并边。
     * @return 合并后的完整根形状。
     *
     * @throws std::invalid_argument 输入为空、边数量不足、存在重复边或边不属于 root。
     * @throws std::runtime_error 所选边不连续、不同域、未能完整合并或结果拓扑无效。
     */
    static TopoDS_Shape mergeEdges(
        const TopoDS_Shape& root,
        const std::vector<TopoDS_Edge>& edges);

    /**
     * @brief 使用已经位于目标面上的几何边分割一个 Face。
     *
     * 目标面可以是根形状的直接子形状，也可以嵌套在 Shell 或 Solid 中。分割边必须
     * 已经落在目标面的参数域内，且相互连接时共享拓扑顶点；本函数不负责投影或压印。
     *
     * @param root 当前几何根形状。
     * @param target_face 要分割的面，必须属于 root。
     * @param splitting_edges 一条或多条位于 target_face 上的分割边。
     * @return 分割后的完整根形状。
     *
     * @throws std::invalid_argument 输入为空、分割边集合为空、目标面不属于 root，或分割边
     * 不在目标面上。
     * @throws std::runtime_error OCC 分割失败、目标面没有实际分裂或结果拓扑无效。
     */
    static TopoDS_Shape splitFace(
        const TopoDS_Shape& root,
        const TopoDS_Face& target_face,
        const std::vector<TopoDS_Edge>& splitting_edges);

    /**
     * @brief 使用一个或多个相交 Face 分割目标 Face。
     *
     * 切割面只作为工具保留在原位置；函数先计算目标面与切割面的交线，再仅替换
     * 目标面，避免同时分割根形状中的其他 Face。
     *
     * @param root 当前几何根形状。
     * @param target_face 要分割的面，必须属于 root。
     * @param splitting_faces 一个或多个属于 root 的切割面。
     * @return 分割后的完整根形状。
     */
    static TopoDS_Shape splitFaceByFaces(
        const TopoDS_Shape& root,
        const TopoDS_Face& target_face,
        const std::vector<TopoDS_Face>& splitting_faces);

    /**
     * @brief 查找给定容差内方向相容的跨面自由边配对。
     *
     * 以较短边为基准计算到较长边的最大采样偏差，因此能够识别“一条长边对应
     * 多条短边”的 Stitch 场景；同一 Face 内的边和已经共享拓扑点的边不会入选。
     *
     * @param root 当前几何根形状。
     * @param tolerance 最大允许间隙，使用模型长度单位。
     */
    static std::vector<GeometryStitchCandidate> findStitchCandidates(
        const TopoDS_Shape& root,
        double tolerance);

    /**
     * @brief 自动缝合根形状中位于给定容差内的跨面自由边。
     *
     * @return 修复后的完整根形状以及候选数、实际 Stitch 边数。
     */
    static GeometryGapRepairResult repairFreeEdgeGaps(
        const TopoDS_Shape& root,
        double tolerance);

    /**
     * @brief 在指定容差内缝合两组自由边链。
     *
     * 仅把两组选择链的相邻 Face 加入局部 Sewing，根形状中已有连接保持不变；支持
     * 一条长边与多条连续短边的自动切分和共享。两组边都必须属于 root、各自连续，
     * 且每条边只能邻接一个 Face。
     *
     * @param root 当前几何根形状。
     * @param first_chain 第一组自由边链。
     * @param second_chain 第二组自由边链。
     * @param tolerance 最大缝合距离，必须为有限正数。
     * @return 缝合后的完整根形状。
     *
     * @throws std::invalid_argument 输入为空、重复、非连续、不是自由边或不属于 root。
     * @throws std::runtime_error 两组边不在容差内、未完整缝合或结果拓扑无效。
     */
    static TopoDS_Shape stitchBoundaryEdges(
        const TopoDS_Shape& root,
        const std::vector<TopoDS_Edge>& first_chain,
        const std::vector<TopoDS_Edge>& second_chain,
        double tolerance);

    /**
     * @brief 从种子自由边扩展出容差内可缝合的同侧连续自由边链。
     *
     * 只吸收「在容差内存在跨面自由边配对」的邻接自由边，避免把同一面的
     * 无关外轮廓扩进间隙边界。种子边自身必须是自由边且存在对侧配对。
     *
     * @param root 当前几何根形状。
     * @param seed 间隙边，必须是 root 上的自由边界边。
     * @param tolerance 最大间隙距离，必须为有限正数。
     * @return 含种子边在内的同侧可缝合自由边链。
     *
     * @throws std::invalid_argument 种子边不是自由边或不属于 root。
     * @throws std::runtime_error 种子边在容差内没有对侧自由边配对。
     */
    static std::vector<TopoDS_Edge> expandStitchableFreeChain(
        const TopoDS_Shape& root,
        const TopoDS_Edge& seed,
        double tolerance);

    /**
     * @brief 为种子自由边链查找容差内的对侧间隙链，按最大间隙升序返回。
     *
     * 候选链须与种子链互相覆盖且总长接近，语义与 stitchBoundaryEdges 一致；
     * 多组时由调用方按容差收窄，或取间隙最小者。不执行缝合。
     *
     * @param root 当前几何根形状。
     * @param seed_chain 同侧连续自由边链。
     * @param tolerance 最大间隙距离，必须为有限正数。
     * @return 对侧链候选；第一项最大间隙最小。
     */
    static std::vector<GeometryGapPartnerChain> findGapPartnerChains(
        const TopoDS_Shape& root,
        const std::vector<TopoDS_Edge>& seed_chain,
        double tolerance);

    /**
     * @brief 以种子自由边为入口，自动识别所属间隙边界并缝合已有面。
     *
     * 只做已有面自由边 Sewing，不创建填充面；多组对侧候选时取最大间隙最小者。
     * 等价于 expandStitchableFreeChain + findGapPartnerChains + stitchBoundaryEdges。
     *
     * @param root 当前几何根形状。
     * @param seed_edge 间隙边，必须是 root 上的自由边界边。
     * @param tolerance 最大缝合距离，必须为有限正数。
     * @return 缝合后的完整根形状。
     *
     * @throws std::invalid_argument 种子边不是自由边或不属于 root。
     * @throws std::runtime_error 容差内找不到对侧间隙链，或缝合失败。
     */
    static TopoDS_Shape stitchGapFromSeedEdge(
        const TopoDS_Shape& root,
        const TopoDS_Edge& seed_edge,
        double tolerance);

    /**
     * @brief 从自由边识别孔洞或两侧间隙，创建并连接一个平面或非共面填充面。
     * @param root 当前几何根形状。
     * @param seed_edge 所属边界的种子自由边。
     * @param tolerance 对侧间隙的搜索距离和曲面拟合误差上限，必须有限且不小于 OCC 几何精度。
     * @return 新增一个面后的完整根形状；原始形状不被原地修改。
     * @throws std::runtime_error 边界歧义、不闭合、覆盖已有面或无法生成有效补面。
     */
    static TopoDS_Shape fillGapFromSeedEdge(
        const TopoDS_Shape& root,
        const TopoDS_Edge& seed_edge,
        double tolerance);

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

    /**
     * @brief 从根形状中删除一个几何形状，Face 支持嵌套在 Shell/Solid 内。
     *
     * 顶层独立形状走 removeTopLevelShape；嵌套 Face 从父 Shell/Solid 中摘除后重建
     * 父级，Solid 因缺面不再闭合时降级为对应 Shell。用于切除分割后的突出面片。
     * 嵌套的 Edge/Vertex 不支持（会破坏所在 Face 的 Wire），请先删所属 Face。
     *
     * @param root 当前几何根形状。
     * @param target 要删除的 Vertex、Edge、Face 或 Solid。
     * @param delete_children 为 false 时保留独占下级拓扑；嵌套面独占的边提升为独立几何。
     * @return 删除后的根形状；没有任何剩余拓扑时返回空 Shape。
     *
     * @throws std::invalid_argument 输入为空、类型不支持、目标不属于 root，或目标是
     * 不能单独删除的嵌套 Edge/Vertex。
     * @throws std::runtime_error 编辑后的拓扑无效。
     */
    static TopoDS_Shape removeShape(
        const TopoDS_Shape& root,
        const TopoDS_Shape& target,
        bool delete_children);
};
