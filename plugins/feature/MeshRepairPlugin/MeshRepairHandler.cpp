/**
 * @file MeshRepairHandler.cpp
 * @brief 网格修复处理器：基于 CGAL PMP 的补洞、自交检测与退化清理
 */

// CGAL 头文件必须置于所有 OCC 相关头文件之前：
// OCC 的 Standard_Handle.hxx 将 Handle 定义为宏（Handle(X) -> opencascade::handle<X>），
// 若先引入 OCC，宏会污染后续解析的 CGAL/Handle.h 类声明，造成大片级联语法错误
#include <CGAL/boost/graph/border.h>
#include <CGAL/Polygon_mesh_processing/repair_degeneracies.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>

#include "MeshRepairHandler.h"

#include "ArgObject.h"
#include "ArgType.h"
#include "CgalExceptionGuard.h"
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
 * @brief 通过 ComponentOperator::replaceMesh 写回 CGAL 修复结果
 *
 * fromSurfaceMesh 输出临时 MeshData（vertex_positions_ 自包含）；replaceMesh 内部
 * 完成 gid 释放/重建与 Topology 标脏，通知由 FeatureSystem::invoke 边界 flush 统一发出。
 * fromSurfaceMesh 内部异常会向外抛出，由 execute 的 try/catch 兜底，此处不再做
 * 错误码返回（保留 bool 返回值会导致调用点出现永远不进入的死分支）。
 */
void writeBack(ComponentOperator& comp_op, const CgalMesh& sm)
{
    auto out_mesh = std::make_unique<MeshData>();
    fromSurfaceMesh(sm, *out_mesh);
    comp_op.replaceMesh(std::move(out_mesh));
}

/**
 * @brief 三角化所有边界环（孔洞），逐环跟踪成功/失败计数
 *
 * 部分孔洞因非流形无法三角化时返回 "已修补 M/N 个孔洞"，避免原实现仅看 new_faces
 * 是否非空就视为全部成功的误报；孔洞的逐次三角化是幂等的（CGAL 仅增面、不删旧）。
 *
 * @note 非原子性语义：
 *       - border_cycles 在循环前一次性提取；CGAL 增量加面通常不使旧 halfedge 失效，
 *         因此本实现不在每次迭代前用 is_border() 复核（出于性能考虑）。
 *       - 若中途某环触发 CGAL 断言（已被 CgalExceptionGuard 转抛 C++ 异常），
 *         已成功三角化的环留在 sm 中但尚未写回组件——功能表现即为"整体失败、
 *         网格未更新"，属于部分修改被丢弃的预期行为。
 */
std::string fillHoles(ComponentOperator& comp_op, CgalMesh& sm)
{
    namespace PMP = CGAL::Polygon_mesh_processing;

    std::vector<CgalMesh::Halfedge_index> border_cycles;
    CGAL::extract_boundary_cycles(sm, std::back_inserter(border_cycles));
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

    writeBack(comp_op, sm);

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
 *
 * @note 计数口径：removed = before - sm.number_of_faces()，即"面数差值"。
 *       PMP::remove_degenerate_faces 除删除退化面外还会顺带清理退化边可能产生的
 *       孤立结构，因此差值是面数变化量而非严格意义上的"被删面数"。作为 UI 文案
 *       的"已移除 X 个退化面"近似足够；如需精确口径请自行在调用点重新统计。
 */
std::string removeDegenerateFaces(ComponentOperator& comp_op, CgalMesh& sm)
{
    namespace PMP = CGAL::Polygon_mesh_processing;

    const std::size_t before = sm.number_of_faces();
    PMP::remove_degenerate_faces(sm);
    const std::size_t removed = before - sm.number_of_faces();
    if (removed == 0)
        return std::string("未发现退化面");

    writeBack(comp_op, sm);

    std::ostringstream oss;
    oss << "已移除 " << removed << " 个退化面，网格已更新";
    return oss.str();
}

/**
 * @brief 判断 MeshData 是否含体单元（约定：solid_*_offset_.size() > 1 即视为"有体"）
 */
bool hasSolids(const MeshData& mesh)
{
    return mesh.solid_vertices_offset_.size() > 1
        || mesh.solid_faces_offset_.size() > 1
        || mesh.solid_faces_vertices_offset_.size() > 1;
}

/**
 * @brief 判断 MeshData 是否全三角面（任一非三角面即返回 false）
 *
 * 按 MeshData 约定：face_vertices_offset_[i+1] - face_vertices_offset_[i] 为面 i 的顶点数。
 */
bool isAllTriangular(const MeshData& mesh)
{
    for (std::size_t i = 0; i + 1 < mesh.face_vertices_offset_.size(); ++i) {
        if (mesh.face_vertices_offset_[i + 1] - mesh.face_vertices_offset_[i] != 3)
            return false;
    }
    return true;
}

} // namespace

void MeshRepairHandler::setup(FeatureRegistrar& reg, FeatureContext&)
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

    // 入口预检：PMP / Surface_mesh 普遍要求纯三角表面网格。
    // - 含体单元：toSurfaceMesh 会一并丢弃 solid_* 字段，结果失真；此处直接拒收。
    // - 含非三角面：toSurfaceMesh 内部已 throw；此处先一步拦下，给出更直白的文案。
    // 两条都满足后才进入 CGAL 路径，避免在 PMP 内部才暴露失败。
    if (hasSolids(mesh))
        return std::string("网格修复仅支持纯三角表面网格（当前 Component 含体单元，请改用其他工具）");
    if (!isAllTriangular(mesh))
        return std::string("网格修复仅支持纯三角表面网格（当前 Component 含非三角面，请先转换为全三角网格）");

    // 作用域内 CGAL 断言失败 → 抛 C++ 异常（详见 CgalExceptionGuard 注释）
    cgalsupport::CgalExceptionGuard cgal_guard;

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