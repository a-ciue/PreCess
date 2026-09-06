/**
 * @file MeshBooleanHandler.cpp
 * @brief 网格布尔处理器：基于 CGAL PMP corefinement 的交/并/差运算
 *
 * 内核选择说明：布尔运算（corefinement）的求交点是"构造"，EPIC 下交点坐标
 * 会被 double 舍入，可能引发输出面自相交、非流形等错误；本项目依赖未启用
 * GMP/MPFR（无外部依赖），EPECK 退化为 Lazy_exact_nt<Quotient<MP_Float>>，
 * 正确性不变、性能下降，网格规模不大时仍可接受，因此布尔路径统一走精确内核。
 */

// CGAL 头文件必须置于所有 OCC 相关头文件之前：
// OCC 的 Standard_Handle.hxx 将 Handle 定义为宏（Handle(X) -> opencascade::handle<X>），
// 若先引入 OCC，宏会污染后续解析的 CGAL/Handle.h 类声明，造成大片级联语法错误
#include <CGAL/boost/graph/helpers.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/internal/Corefinement/Self_intersection_exception.h>
#include <CGAL/Side_of_triangle_mesh.h>

#include "MeshBooleanHandler.h"

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

//! @brief 参数下标：对象 A / 对象 B 选择器、运算类型
constexpr std::size_t kComponentAParam = 0;
constexpr std::size_t kComponentBParam = 1;
constexpr std::size_t kOperationParam = 2;

//! @brief 运算类型枚举（与 Combo 字符串顺序一致）
enum class BoolOp {
    Union = 0,
    Intersection = 1,
    DifferenceAB = 2, //!< A − B
    DifferenceBA = 3, //!< B − A
};

//! @brief 运算的显示名（用于结果文案）
const char* boolOpDisplayName(BoolOp op)
{
    switch (op) {
    case BoolOp::Union:          return "并集";
    case BoolOp::Intersection:   return "交集";
    case BoolOp::DifferenceAB:   return "差集(A−B)";
    case BoolOp::DifferenceBA:   return "差集(B−A)";
    }
    return "未知运算";
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

//! @brief 解析某个 Selector 参数为单个 Component id；失败时通过 out_err 给出面向用户的原因
std::optional<Index> resolveSingleComponent(const FeatureContext& ctx,
    std::size_t param_index, const char* label, std::string& out_err)
{
    const auto* selection_ptr = ctx.params.value(param_index).get<ArgTypeEnum::Selector>();
    if (!selection_ptr || !*selection_ptr) {
        out_err = std::string("请选择对象 ") + label + "（Component）";
        return std::nullopt;
    }
    const Selection& selection = **selection_ptr;
    if (selection.type != ElementEnum::Component) {
        out_err = std::string("对象 ") + label + " 的选择类型不是 Component，请重新选择";
        return std::nullopt;
    }
    if (selection.ids.size() != 1) {
        out_err = std::string("请为对象 ") + label + " 选择且仅选择一个 Component";
        return std::nullopt;
    }
    return selection.ids.front();
}

/**
 * @brief 网格级入口预检：布尔运算仅接受"纯三角、无体单元"的表面网格
 * @return 非空表示存在拒绝原因（不进入 CGAL 路径）；为空表示通过
 */
std::optional<std::string> surfaceMeshPrecheck(const MeshData& mesh, const char* label)
{
    if (hasSolids(mesh))
        return std::string("网格布尔仅支持纯三角表面网格：对象 ") + label
            + " 含体单元，请改用其他工具";
    if (!isAllTriangular(mesh))
        return std::string("网格布尔仅支持纯三角表面网格：对象 ") + label
            + " 含非三角面，请先转换为全三角网格";
    return std::nullopt;
}

//! @brief 目标对象名（用于结果文案）
std::string componentLabel(const ComponentOperator& comp_op, const char* label)
{
    return std::string("对象 ") + label + "（" + comp_op.component().name + "）";
}

//! @brief 组装"已写回对象 A"的成功文案：布尔结果 → A 网格 → replaceMesh
std::string buildSuccessText(BoolOp op, ComponentOperator& comp_op_a)
{
    const auto* mesh = comp_op_a.mesh();
    const std::size_t faces = mesh && !mesh->face_vertices_offset_.empty()
        ? mesh->face_vertices_offset_.size() - 1 : 0;
    std::ostringstream oss;
    oss << boolOpDisplayName(op) << "完成："
        << componentLabel(comp_op_a, "A")
        << " 的网格已更新（顶点 " << (mesh ? mesh->vertex_count_ : 0)
        << " / 三角面 " << faces << "）";
    return oss.str();
}

//! @brief 布尔结果网格写回对象 A（ComponentOperator::replaceMesh 内建 gid 纪律与写前标脏）
void writeBackExact(ComponentOperator& comp_op_a, const CgalExactMesh& result)
{
    auto out_mesh = std::make_unique<MeshData>();
    fromSurfaceMesh(result, *out_mesh);
    comp_op_a.replaceMesh(std::move(out_mesh));
}

/**
 * @brief 把两个自包含的表面网格合并为单个 MeshData（两个独立壳体共存）
 *
 * 用于两表面不相交场景：
 *   - 并集：两个分离壳体 → 单个组件内的两个闭合壳体
 *   - A−B 且 B 完全在 A 内部：A 被挖空成空腔 → ∂A ∪ ∂B 两层壳体
 * 输出仅保留顶点与面连通性（语义同 fromSurfaceMesh 输出）。
 *
 * MeshData 的 face_vertices_offset_ 约定：以 0 起始、以总顶点数结尾（首尾均为哨兵）。
 * 合并时 rhs 的首个哨兵 0 平移后恰好等于 lhs 末尾的"总顶点数"，须跳过，避免
 * 产生 0 顶点的空面区间。
 */
std::unique_ptr<MeshData> mergeSurfaceShells(const MeshData& lhs, const MeshData& rhs)
{
    auto out = std::make_unique<MeshData>();
    out->solid_vertices_offset_ = { 0 };
    out->solid_faces_vertices_offset_ = { 0 };
    out->solid_faces_offset_ = { 0 };

    const Index base = static_cast<Index>(lhs.vertex_positions_.size());
    out->vertex_positions_ = lhs.vertex_positions_;
    out->vertex_positions_.insert(out->vertex_positions_.end(),
        rhs.vertex_positions_.begin(), rhs.vertex_positions_.end());
    out->vertex_count_ = static_cast<Index>(out->vertex_positions_.size());

    // lhs 的面偏移原样保留；rhs 跳过首个哨兵再整体平移，避免首尾哨兵重复
    const Index face_base = static_cast<Index>(lhs.face_vertices_.size());
    out->face_vertices_offset_.reserve(
        lhs.face_vertices_offset_.size() + rhs.face_vertices_offset_.size() - 1);
    for (Index off : lhs.face_vertices_offset_)
        out->face_vertices_offset_.push_back(off);
    for (std::size_t i = 1; i < rhs.face_vertices_offset_.size(); ++i)
        out->face_vertices_offset_.push_back(rhs.face_vertices_offset_[i] + face_base);

    out->face_vertices_ = lhs.face_vertices_;
    out->face_vertices_.reserve(lhs.face_vertices_.size() + rhs.face_vertices_.size());
    for (Index id : rhs.face_vertices_)
        out->face_vertices_.push_back(id + base);

    return out;
}

/**
 * @brief 以对象 B 的网格整体替换对象 A（用于"并集结果即 B / 交集结果即 B"等场景）
 */
void writeBackCopyOfB(ComponentOperator& comp_op_a, const MeshData& mesh_b)
{
    comp_op_a.replaceMesh(mesh_b.clone());
}

/**
 * @brief 布尔运算主路径：两表面相交 → CGAL corefinement 精确求解
 *
 * @pre 已通过闭合/自交/朝向规整校验；两表面存在相交
 */
std::string computeIntersecting(BoolOp op, ComponentOperator& comp_op_a,
    CgalExactMesh& sm_a, CgalExactMesh& sm_b)
{
    namespace PMP = CGAL::Polygon_mesh_processing;

    CgalExactMesh out;
    // np1 开启 throw_on_self_intersection：corefinement 在输入面彼此相交带内发现
    // 自交时抛 Self_intersection_exception（见 execute 的 catch），而非产出错误结果
    const auto params = CGAL::parameters::throw_on_self_intersection(true);

    bool ok = false;
    switch (op) {
    case BoolOp::Union:
        ok = PMP::corefine_and_compute_union(sm_a, sm_b, out, params);
        break;
    case BoolOp::Intersection:
        ok = PMP::corefine_and_compute_intersection(sm_a, sm_b, out, params);
        break;
    case BoolOp::DifferenceAB:
        ok = PMP::corefine_and_compute_difference(sm_a, sm_b, out, params);
        break;
    case BoolOp::DifferenceBA:
        ok = PMP::corefine_and_compute_difference(sm_b, sm_a, out, params);
        break;
    }

    if (!ok || out.number_of_vertices() == 0 || out.number_of_faces() == 0) {
        return std::string("布尔运算失败：结果为空或将为非流形结构"
            "（两对象可能仅相切/共面接触），网格未修改");
    }

    writeBackExact(comp_op_a, out);
    return buildSuccessText(op, comp_op_a);
}

/**
 * @brief 退化场景：两表面互不相交 → 按"体积包含关系"给出语义正确的结果
 *
 * 输入为闭合壳体且边界互不相交时，二者关系只可能是：A 含 B、B 含 A、相互分离
 * （凭"一顶点是否落入另一闭合壳体内部"判定，边界无交点时整壳同侧，取首顶点即可）。
 * 每种运算按体积语义推导结果；结果等于 A/B 或为空时不修改网格，仅提示。
 */
std::string computeNonIntersecting(BoolOp op, ComponentOperator& comp_op_a,
    const MeshData& mesh_a, const MeshData& mesh_b,
    const CgalExactMesh& sm_a, const CgalExactMesh& sm_b)
{
    const CgalExactPoint3 p_a = sm_a.point(*sm_a.vertices().begin());
    const CgalExactPoint3 p_b = sm_b.point(*sm_b.vertices().begin());

    const bool b_inside_a
        = CGAL::Side_of_triangle_mesh<CgalExactMesh, CgalExactKernel>(sm_a)(p_b)
            == CGAL::ON_BOUNDED_SIDE;
    const bool a_inside_b
        = CGAL::Side_of_triangle_mesh<CgalExactMesh, CgalExactKernel>(sm_b)(p_a)
            == CGAL::ON_BOUNDED_SIDE;

    const char* sep_note = "两对象互不相交";

    // 相互分离（两壳无包含关系）
    if (!b_inside_a && !a_inside_b) {
        switch (op) {
        case BoolOp::Union: {
            comp_op_a.replaceMesh(mergeSurfaceShells(mesh_a, mesh_b));
            std::string text = buildSuccessText(op, comp_op_a);
            return text + "（" + sep_note + "，结果含两个独立壳体）";
        }
        case BoolOp::Intersection:
            return std::string("交集为空：") + sep_note + "，网格未修改";
        case BoolOp::DifferenceAB:
            return std::string("差集(A−B) 即对象 A：") + sep_note + "，网格未修改";
        case BoolOp::DifferenceBA:
            writeBackCopyOfB(comp_op_a, mesh_b);
            return std::string("差集(B−A) 即对象 B：") + sep_note
                + "，已将结果写回对象 A";
        }
    }

    // B 完全位于 A 内部（A 含 B）
    if (b_inside_a) {
        switch (op) {
        case BoolOp::Union:
            return std::string("并集结果即对象 A：对象 B 完全位于对象 A 内部，网格未修改");
        case BoolOp::Intersection:
            writeBackCopyOfB(comp_op_a, mesh_b);
            return std::string("交集结果即对象 B：对象 B 完全位于对象 A 内部，已写回对象 A");
        case BoolOp::DifferenceAB:
            comp_op_a.replaceMesh(mergeSurfaceShells(mesh_a, mesh_b));
            return buildSuccessText(op, comp_op_a)
                + "（对象 B 完全位于对象 A 内部，结果为挖去 B 的空腔壳体）";
        case BoolOp::DifferenceBA:
            return std::string("差集(B−A) 为空：对象 B 完全位于对象 A 内部，网格未修改");
        }
    }

    // A 完全位于 B 内部（B 含 A）
    switch (op) {
    case BoolOp::Union:
        writeBackCopyOfB(comp_op_a, mesh_b);
        return std::string("并集结果即对象 B：对象 A 完全位于对象 B 内部，已写回对象 A");
    case BoolOp::Intersection:
        return std::string("交集结果即对象 A：对象 A 完全位于对象 B 内部，网格未修改");
    case BoolOp::DifferenceAB:
        return std::string("差集(A−B) 为空：对象 A 完全位于对象 B 内部，网格未修改");
    case BoolOp::DifferenceBA:
        comp_op_a.replaceMesh(mergeSurfaceShells(mesh_b, mesh_a));
        return buildSuccessText(op, comp_op_a)
            + "（对象 A 完全位于对象 B 内部，结果为挖去 A 的空腔壳体）";
    }

    return std::string("未知运算");
}

} // namespace

void MeshBooleanHandler::setup(FeatureRegistrar& reg, FeatureContext&)
{
    reg.addParameter({
        ArgTypeEnum::Selector,
        "对象 A",
        "Component",
        "选择第一个操作对象：布尔运算结果将写回该对象（建议先选被保留的一方）",
    });
    reg.addParameter({
        ArgTypeEnum::Selector,
        "对象 B",
        "Component",
        "选择第二个操作对象",
    });
    reg.addParameter({
        ArgTypeEnum::Combo,
        "布尔运算",
        "并集,交集,差集(A−B),差集(B−A)|0",
        "选择布尔运算类型：并集 / 交集 / 差集(A−B) / 差集(B−A)",
    });
    reg.addMenuItem({ "功能/网格", "网格布尔" });
}

std::any MeshBooleanHandler::execute(FeatureContext& ctx)
{
    // 两个 Component 均由 Selector 参数显式解析（AGENTS.md §10：不依赖对象树选中态）
    std::string sel_err;
    const auto a_id = resolveSingleComponent(ctx, kComponentAParam, "A", sel_err);
    if (!a_id)
        return sel_err;
    const auto b_id = resolveSingleComponent(ctx, kComponentBParam, "B", sel_err);
    if (!b_id)
        return sel_err;

    if (*a_id == *b_id)
        return std::string("对象 A 与对象 B 相同，请选择两个不同的 Component");

    auto comp_op_a = ctx.componentOperator ? ctx.componentOperator(*a_id) : std::nullopt;
    if (!comp_op_a || !comp_op_a->mesh())
        return std::string("对象 A 没有网格数据（网格布尔仅支持网格模型）");
    auto comp_op_b = ctx.componentOperator ? ctx.componentOperator(*b_id) : std::nullopt;
    if (!comp_op_b || !comp_op_b->mesh())
        return std::string("对象 B 没有网格数据（网格布尔仅支持网格模型）");

    int op_index = 0;
    if (const auto* v = ctx.params.value(kOperationParam).get<ArgTypeEnum::Combo>())
        op_index = *v;
    if (op_index < static_cast<int>(BoolOp::Union)
        || op_index > static_cast<int>(BoolOp::DifferenceBA)) {
        return std::string("错误：未知的布尔运算类型");
    }
    const auto op = static_cast<BoolOp>(op_index);

    const MeshData& mesh_a = *comp_op_a->mesh();
    const MeshData& mesh_b = *comp_op_b->mesh();

    // 入口预检：corefinement / Surface_mesh 普遍要求纯三角表面网格
    if (auto err = surfaceMeshPrecheck(mesh_a, "A"))
        return *err;
    if (auto err = surfaceMeshPrecheck(mesh_b, "B"))
        return *err;

    // 作用域内 CGAL 断言失败 → 抛 C++ 异常（详见 CgalExceptionGuard 注释）
    cgalsupport::CgalExceptionGuard cgal_guard;

    try {
        // 精确内核：布尔求交为精确构造，避免 EPIC 的 double 舍入污染结果
        CgalExactMesh sm_a = toExactSurfaceMesh(mesh_a);
        CgalExactMesh sm_b = toExactSurfaceMesh(mesh_b);

        const std::size_t faces_a = mesh_a.face_vertices_offset_.empty()
            ? 0 : mesh_a.face_vertices_offset_.size() - 1;
        const std::size_t faces_b = mesh_b.face_vertices_offset_.empty()
            ? 0 : mesh_b.face_vertices_offset_.size() - 1;
        if (sm_a.number_of_faces() != faces_a)
            return std::string("对象 A 的网格含非流形边（一条边被多于两个面共用），不支持布尔运算");
        if (sm_b.number_of_faces() != faces_b)
            return std::string("对象 B 的网格含非流形边（一条边被多于两个面共用），不支持布尔运算");

        // corefinement 以"网格包围的体积"为操作对象：要求闭合（水密）三角网格
        if (!CGAL::is_closed(sm_a))
            return std::string("对象 A 不是闭合（水密）网格：布尔运算要求两个闭合三角网格，"
                "请先使用「网格修复 - 补洞」或重新网格化");
        if (!CGAL::is_closed(sm_b))
            return std::string("对象 B 不是闭合（水密）网格：布尔运算要求两个闭合三角网格，"
                "请先使用「网格修复 - 补洞」或重新网格化");

        // 自交网格会令 corefinement 输出错误结果或触发自交异常，先拦下给出可定位提示
        if (CGAL::Polygon_mesh_processing::does_self_intersect(sm_a))
            return std::string("对象 A 存在自相交面：请先使用「网格修复 - 自交检测」定位并修复");
        if (CGAL::Polygon_mesh_processing::does_self_intersect(sm_b))
            return std::string("对象 B 存在自相交面：请先使用「网格修复 - 自交检测」定位并修复");

        // 规整朝向：按"包围体积"语义使各闭合壳体朝外（布尔运算的前置约定）
        CGAL::Polygon_mesh_processing::orient_to_bound_a_volume(sm_a);
        CGAL::Polygon_mesh_processing::orient_to_bound_a_volume(sm_b);

        // 两表面相交 → corefinement 主路径；不相交 → 包含/分离退化场景
        if (CGAL::Polygon_mesh_processing::do_intersect(sm_a, sm_b))
            return computeIntersecting(op, *comp_op_a, sm_a, sm_b);
        return computeNonIntersecting(op, *comp_op_a, mesh_a, mesh_b, sm_a, sm_b);
    } catch (const CGAL::Polygon_mesh_processing::Corefinement::Self_intersection_exception& e) {
        // 输入在相交带内存在自交时由 throw_on_self_intersection(true) 抛出
        spdlog::error("[MeshBoolean] 自交异常: op={}, error={}", boolOpDisplayName(op), e.what());
        return std::string("网格布尔失败：输入网格在相交区域检测到自相交，网格未修改");
    } catch (const CGAL::Failure_exception& e) {
        // CGAL 内部拓扑/几何不变量违反（如 non-manifold）：给 UI 用户温和文案，细节留日志
        spdlog::error("[MeshBoolean] CGAL 拓扑不变量违反: op={}, lib={}, expr={}, file={}:{}",
            boolOpDisplayName(op), e.library(), e.expression(), e.filename(), e.line_number());
        return std::string("网格布尔失败：当前网格拓扑不符合运算前提（建议检查是否存在非流形结构）");
    } catch (const std::exception& e) {
        spdlog::error("[MeshBoolean] CGAL 操作异常: op={}, error={}", boolOpDisplayName(op), e.what());
        return std::string("网格布尔失败：") + e.what();
    }
}

} // namespace systems::feature
