/**
 * @file MeshRepairHandler.cpp
 * @brief 网格修复处理器：基于 CGAL PMP 的补洞、自交检测与退化清理
 */

// CGAL 头文件必须置于所有 OCC 相关头文件之前：
// OCC 的 Standard_Handle.hxx 将 Handle 定义为宏（Handle(X) -> opencascade::handle<X>），
// 若先引入 OCC，宏会污染后续解析的 CGAL/Handle.h 类声明，造成大片级联语法错误
#include <CGAL/Polygon_mesh_processing/border.h>
#include <CGAL/Polygon_mesh_processing/repair_degeneracies.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>

#include <CGAL/assertions_behaviour.h>
#include <CGAL/exceptions.h>

#include "MeshRepairHandler.h"

#include "ArgObject.h"
#include "ArgType.h"
#include "CgalMeshAdapter.h"
#include "ComponentData.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "MeshData.h"
#include "Selection.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <optional>
#include <sstream>
#include <string>

namespace systems::feature {

namespace {

//! @brief 参数下标：目标 Component 选择器、操作类型
constexpr std::size_t kComponentParam = 0;
constexpr std::size_t kOperationParam = 1;

//! @brief 操作类型枚举（与 Combo 字符串顺序一致）
enum class Operation {
    FillHoles = 0,
    DetectSelfIntersections = 1,
    RemoveDegenerateFaces = 2,
};

/**
 * @brief RAII 守卫：作用域内将 CGAL 断言失败行为改为抛 C++ 异常
 *
 * CGAL 默认失败行为是 ABORT（直接 std::abort + stderr 打印内部表达式），对 UI
 * 用户极不友好。在本功能执行期间临时切到 THROW_EXCEPTION，使所有
 * CGAL_assertion / CGAL_error 触发时抛 CGAL::Failure_exception 链（如
 * Assertion_exception / Precondition_exception），被外层 try/catch 统一
 * 捕获并转成温和文案。作用域结束自动恢复 ABORT，避免影响其他使用 CGAL 的代码。
 */
class CgalExceptionGuard {
public:
    CgalExceptionGuard()
        : prev_behaviour_(CGAL::set_error_behaviour(CGAL::THROW_EXCEPTION))
    {
    }
    ~CgalExceptionGuard()
    {
        CGAL::set_error_behaviour(prev_behaviour_);
    }
    CgalExceptionGuard(const CgalExceptionGuard&) = delete;
    CgalExceptionGuard& operator=(const CgalExceptionGuard&) = delete;

private:
    CGAL::Failure_behaviour prev_behaviour_;
};

/**
 * @brief 通过 ComponentOperator::replaceMesh 写回 CGAL 修复结果
 *
 * fromSurfaceMesh 输出临时 MeshData（vertex_positions_ 自包含）；replaceMesh 内部
 * 完成 gid 释放/重建与 Topology 标脏，通知由 FeatureSystem::invoke 边界 flush 统一发出。
 * @return 写回成功返回 true
 */
bool writeBack(ComponentOperator& comp_op, const CgalMesh& sm)
{
    auto out_mesh = std::make_unique<MeshData>();
    fromSurfaceMesh(sm, *out_mesh);
    comp_op.replaceMesh(std::move(out_mesh));
    return true;
}

/**
 * @brief 三角化所有边界环（孔洞），逐环跟踪成功/失败计数
 *
 * 部分孔洞因非流形无法三角化时返回 "已修补 M/N 个孔洞"，避免原实现仅看 new_faces
 * 是否非空就视为全部成功的误报；孔洞的逐次三角化是幂等的（CGAL 仅增面、不删旧）。
 */
std::string fillHoles(ComponentOperator& comp_op, CgalMesh& sm)
{
    namespace PMP = CGAL::Polygon_mesh_processing;

    std::vector<CgalMesh::Halfedge_index> border_cycles;
    PMP::extract_boundary_cycles(sm, std::back_inserter(border_cycles));
    if (border_cycles.empty())
        return std::string("网格无孔洞（未发现边界环）");

    std::vector<CgalMesh::Face_index> new_faces;
    std::size_t success_count = 0;
    for (const auto h : border_cycles) {
        std::vector<CgalMesh::Face_index> hole_faces;
        PMP::triangulate_hole(sm, h,
            CGAL::parameters::face_output_iterator(std::back_inserter(hole_faces)));
        if (!hole_faces.empty()) {
            ++success_count;
            new_faces.insert(new_faces.end(), hole_faces.begin(), hole_faces.end());
        }
    }

    if (success_count == 0)
        return std::string("孔洞修补失败（边界可能为非流形，无法三角化）");

    if (!writeBack(comp_op, sm))
        return std::string("错误：修复结果写回组件失败");

    std::ostringstream oss;
    oss << "已修补 " << success_count << "/" << border_cycles.size()
        << " 个孔洞，新增 " << new_faces.size() << " 个三角面，网格已更新";
    return oss.str();
}

/**
 * @brief 检测自相交面并返回报告（不修改网格）
 */
std::string detectSelfIntersections(const CgalMesh& sm)
{
    namespace PMP = CGAL::Polygon_mesh_processing;

    std::vector<std::pair<CgalMesh::Face_index, CgalMesh::Face_index>> intersecting;
    PMP::self_intersections(sm, std::back_inserter(intersecting));
    if (intersecting.empty())
        return std::string("未发现自相交面");

    std::ostringstream oss;
    oss << "检测到 " << intersecting.size() << " 对自相交面：\n";
    const std::size_t limit = std::min<std::size_t>(intersecting.size(), 10);
    for (std::size_t i = 0; i < limit; ++i)
        oss << "面 " << intersecting[i].first << " × 面 " << intersecting[i].second << "\n";
    if (intersecting.size() > limit)
        oss << "…… 共 " << intersecting.size() << " 对";
    return oss.str();
}

/**
 * @brief 移除退化面并写回
 */
std::string removeDegenerateFaces(ComponentOperator& comp_op, CgalMesh& sm)
{
    namespace PMP = CGAL::Polygon_mesh_processing;

    const std::size_t before = sm.number_of_faces();
    PMP::remove_degenerate_faces(sm);
    const std::size_t removed = before - sm.number_of_faces();
    if (removed == 0)
        return std::string("未发现退化面");

    if (!writeBack(comp_op, sm))
        return std::string("错误：修复结果写回组件失败");

    std::ostringstream oss;
    oss << "已移除 " << removed << " 个退化面，网格已更新";
    return oss.str();
}

} // namespace

void MeshRepairHandler::setup(FeatureRegistrar& reg)
{
    reg.addParameter({
        ArgTypeEnum::Selector,
        "目标 Component",
        "Component",
        "选择要执行修复操作的 Component",
    });
    reg.addParameter({
        ArgTypeEnum::Combo,
        "操作类型",
        "补洞,自交检测,退化清理|0",
        "选择要执行的网格修复操作",
    });
    reg.addMenuItem({ "功能/网格", "网格修复" });
}

std::any MeshRepairHandler::execute(FeatureContext& ctx)
{
    // 目标 Component 由 Selector 参数显式解析（AGENTS.md §10：不依赖对象树选中态）
    const auto* selection_ptr = ctx.params.value(kComponentParam).get<ArgTypeEnum::Selector>();
    if (!selection_ptr || !*selection_ptr) {
        return std::string("请选择一个 Component");
    }
    const Selection& selection = **selection_ptr;
    if (selection.type != ElementEnum::Component || selection.ids.size() != 1) {
        return std::string("请选择一个 Component");
    }
    const Index component_id = selection.ids.front();

    auto comp_op = ctx.componentOperator ? ctx.componentOperator(component_id) : std::nullopt;
    if (!comp_op || !comp_op->mesh()) {
        return std::string("当前 Component 没有网格数据（网格修复仅支持网格模型）");
    }

    int op_index = 0;
    if (const auto* v = ctx.params.value(kOperationParam).get<ArgTypeEnum::Combo>())
        op_index = *v;

    const MeshData& mesh = *comp_op->mesh();

    // 作用域内 CGAL 断言失败 → 抛 C++ 异常（详见 CgalExceptionGuard 注释）
    CgalExceptionGuard cgal_guard;

    try {
        // MeshData 自包含（vertex_positions_ 常驻），转 CGAL 后调 PMP
        CgalMesh sm = toSurfaceMesh(mesh);
        switch (static_cast<Operation>(op_index)) {
        case Operation::FillHoles:
            return fillHoles(*comp_op, sm);
        case Operation::DetectSelfIntersections:
            return detectSelfIntersections(sm);
        case Operation::RemoveDegenerateFaces:
            return removeDegenerateFaces(*comp_op, sm);
        default:
            return std::string("错误：未知操作类型");
        }
    } catch (const CGAL::Failure_exception& e) {
        // PMP 内部拓扑/几何不变量违反（如 non-manifold、self-intersection 探测期
        // 间触发），CGAL::Failure_exception::what() 含内部表达式，对 UI 用户不友好：
        // spdlog 留底便于开发定位，UI 仅返回固定温和文案。
        spdlog::error("[MeshRepair] CGAL 拓扑不变量违反: op_index={}, lib={}, expr={}, file={}:{}",
            op_index, e.library(), e.expression(), e.filename(), e.line_number());
        return std::string("网格修复失败：当前网格拓扑不符合 PMP 前提（建议重新导入或重新网格化）");
    } catch (const std::exception& e) {
        spdlog::error("[MeshRepair] CGAL 操作异常: op_index={}, error={}", op_index, e.what());
        return std::string("网格修复失败：") + e.what();
    }
}

} // namespace systems::feature