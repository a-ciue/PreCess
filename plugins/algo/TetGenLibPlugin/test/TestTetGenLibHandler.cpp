/**
 * @file TestTetGenLibHandler.cpp
 * @brief TetGenLibHandler 单元测试
 */
#include "TetGenLibHandler.h"

#include "ArgObject.h"
#include "ComponentData.h"
#include "MeshData.h"
#include "MeshIDMap.h"
#include "ModelLayer.h"
#include "Selection.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>
#include <vector>

using namespace systems::algo;
using namespace core;

namespace {

/**
 * @brief 构造一个含闭合三角表面（4 顶四面体）的 ComponentData（用于点 gid 分配）
 */
std::unique_ptr<ComponentData> makeTetrahedronComponent(const std::string& name)
{
    auto comp = std::make_unique<ComponentData>();
    comp->name = name;
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 },
    };
    mesh->vertex_count_ = static_cast<Index>(mesh->vertex_positions_.size());
    mesh->face_vertices_ = { 0, 1, 2, 0, 3, 1, 0, 2, 3, 1, 3, 2 };
    mesh->face_vertices_offset_ = { 0, 3, 6, 9, 12 };
    comp->mesh = std::move(mesh);
    return comp;
}

/**
 * @brief 在 ModelLayer 中添加一个含四面体三角表面的 component，并返回 component_id
 */
Index addMeshComponent(ModelLayer& model_layer, const std::string& name)
{
    ComponentDatas components;
    components.push_back(makeTetrahedronComponent(name));
    const Index model_id = model_layer.addModel(name + "_model", std::move(components));
    ModelData* model = model_layer.modelById(model_id);
    REQUIRE(model != nullptr);
    REQUIRE_FALSE(model->componentIds().empty());
    return model->componentIds().front();
}

/**
 * @brief 构造与 TetGenLibHandler::args_type() 等长（8 个）的占位参数向量
 *        第一个元素替换为传入的 Selector；其余 7 个默认 Combo(0)
 */
std::vector<core::ArgObject> makeArgsWithSelector(std::shared_ptr<Selection> selection)
{
    std::vector<core::ArgObject> args;
    args.push_back(core::ArgObject::create<ArgTypeEnum::Selector>(selection));
    for (std::size_t i = 1; i < 8; ++i) {
        args.push_back(core::ArgObject::create<ArgTypeEnum::Combo>(0));
    }
    return args;
}

} // namespace

TEST_CASE("TetGenLibHandler::args_type() returns correct parameter count and types")
{
    TetGenLibHandler handler;
    auto args = handler.args_type();

    REQUIRE(args.size() == 8);

    SECTION("Parameter 0 (Selector): target component via picked vertices")
    {
        CHECK(args[0].type == ArgTypeEnum::Selector);
        CHECK(args[0].name == "目标 component（点选）");
        CHECK(args[0].content == "Vertex");
    }

    SECTION("Parameter 1 (Combo): keep only largest surface shell")
    {
        CHECK(args[1].type == ArgTypeEnum::Combo);
        CHECK(args[1].name == "是否仅使用最大表面壳");
        CHECK(args[1].content == "是,否");
    }

    SECTION("Parameter 2 (Float): quality bound q")
    {
        CHECK(args[2].type == ArgTypeEnum::Float);
        CHECK(args[2].name == "质量参数 q（0表示关闭）");
        CHECK(args[2].content == "1.2");
    }

    SECTION("Parameter 3 (Float): max element volume a")
    {
        CHECK(args[3].type == ArgTypeEnum::Float);
        CHECK(args[3].name == "最大单元体积 a（0表示关闭）");
        CHECK(args[3].content == "0");
    }

    SECTION("Parameter 4 (Combo): preserve original surface")
    {
        CHECK(args[4].type == ArgTypeEnum::Combo);
        CHECK(args[4].name == "是否保留原始表面");
        CHECK(args[4].content == "是,否");
    }

    SECTION("Parameter 5 (Combo): detect self-intersection only")
    {
        CHECK(args[5].type == ArgTypeEnum::Combo);
        CHECK(args[5].name == "是否仅检测自交");
        CHECK(args[5].content == "是,否|1");
    }

    SECTION("Parameter 6 (Combo): output mode")
    {
        CHECK(args[6].type == ArgTypeEnum::Combo);
        CHECK(args[6].name == "输出方式");
        CHECK(args[6].content == "新建模型,替换当前模型");
        CHECK(args[6].desc.find("默认新建模型") != std::string::npos);
    }

    SECTION("Parameter 7 (Text): advanced TetGen switches")
    {
        CHECK(args[7].type == ArgTypeEnum::Text);
        CHECK(args[7].name == "高级 TetGen 参数");
        CHECK(args[7].content == "");
        CHECK(args[7].desc.find("仅靠 switches") != std::string::npos);
    }
}

TEST_CASE("TetGenLibHandler::resolveComponentId - Path A: selection.component_id takes precedence")
{
    // 用户已在对象树或 Selector 中选中了具体 component，但不在视口选点：
    // 算法系统应优先使用 selection.component_id，不再回退到对象树 fallback_component_id。
    ModelLayer model_layer;
    const Index cid_a = addMeshComponent(model_layer, "compA");
    const Index cid_b = addMeshComponent(model_layer, "compB");
    REQUIRE(cid_a != cid_b);

    auto selection_a = std::make_shared<Selection>();
    selection_a->type = ElementEnum::Vertex;
    selection_a->component_id = cid_a;
    selection_a->ids.clear(); // 路径 A 不依赖 ids

    auto args_a = makeArgsWithSelector(selection_a);
    TetGenLibHandler handler;

    // 即便 fallback 指向其他 component，selection.component_id 应被优先生效
    REQUIRE(handler.resolveComponentId(model_layer, cid_b, args_a) == cid_a);

    selection_a->component_id = cid_b;
    args_a = makeArgsWithSelector(selection_a);
    REQUIRE(handler.resolveComponentId(model_layer, cid_a, args_a) == cid_b);
}

TEST_CASE("TetGenLibHandler::resolveComponentId - Path B: vertex gid reverse-resolved via pointIdMap")
{
    // 用户在视口选 vertex（component_id 未由拾取器填写或为 -1）：
    // 算法系统按 gid 经 pointIdMap.getLocal 反查所在 component，多点必须同 component。
    ModelLayer model_layer;
    const Index cid_a = addMeshComponent(model_layer, "compA");
    const Index cid_b = addMeshComponent(model_layer, "compB");
    REQUIRE(cid_a != cid_b);

    // 先取出 compA / compB 实际分配的 gid 起点（addModel 自动 ensurePointGlobalIds）
    const Index gid_a0 = model_layer.findComponent(cid_a)->point_global_ids_[0];
    const Index gid_a1 = model_layer.findComponent(cid_a)->point_global_ids_[1];
    const Index gid_b0 = model_layer.findComponent(cid_b)->point_global_ids_[0];

    TetGenLibHandler handler;

    SECTION("Single vertex from compA resolves to cid_a")
    {
        auto sel = std::make_shared<Selection>();
        sel->type = ElementEnum::Vertex;
        sel->component_id = -1;
        sel->ids = { gid_a0 };
        auto args = makeArgsWithSelector(sel);
        REQUIRE(handler.resolveComponentId(model_layer, -1, args) == cid_a);
    }

    SECTION("Multiple vertices from same component resolve to that component")
    {
        auto sel = std::make_shared<Selection>();
        sel->type = ElementEnum::Vertex;
        sel->component_id = -1;
        sel->ids = { gid_a0, gid_a1 };
        auto args = makeArgsWithSelector(sel);
        REQUIRE(handler.resolveComponentId(model_layer, -1, args) == cid_a);
    }

    SECTION("Vertices across multiple components are rejected")
    {
        auto sel = std::make_shared<Selection>();
        sel->type = ElementEnum::Vertex;
        sel->component_id = -1;
        sel->ids = { gid_a0, gid_b0 };
        auto args = makeArgsWithSelector(sel);
        // 跨 component 必然拒收，与 GmshMeshHandler 一致
        auto resolved = handler.resolveComponentId(model_layer, -1, args);
        CHECK_FALSE(resolved.has_value());
    }

    SECTION("All-invalidated gids fall back to fallback_component_id when available")
    {
        auto sel = std::make_shared<Selection>();
        sel->type = ElementEnum::Vertex;
        sel->component_id = -1;
        // gid_a0 后续被 release 后再反查会失效；这里用一个极大无效 gid 模拟
        sel->ids = { 99999999 };
        auto args = makeArgsWithSelector(sel);
        // 全部 gid 失效 → 落入路径 C（fallback）
        REQUIRE(handler.resolveComponentId(model_layer, cid_a, args) == cid_a);
    }
}

TEST_CASE("TetGenLibHandler::resolveComponentId - Path C: fallback to fallback_component_id")
{
    ModelLayer model_layer;
    const Index cid_a = addMeshComponent(model_layer, "compA");

    TetGenLibHandler handler;

    SECTION("Selection is null → resolveComponentId uses fallback")
    {
        auto args = makeArgsWithSelector(nullptr);
        REQUIRE(handler.resolveComponentId(model_layer, cid_a, args) == cid_a);
    }

    SECTION("Empty selection → resolveComponentId uses fallback")
    {
        auto empty = std::make_shared<Selection>(); // 默认空
        empty->component_id = -1;
        auto args = makeArgsWithSelector(empty);
        REQUIRE(handler.resolveComponentId(model_layer, cid_a, args) == cid_a);
    }

    SECTION("No selection at all (empty args) → resolveComponentId uses fallback")
    {
        // args 长度小于等于 kSelectorParam 视作"选择器参数不存在"，走 fallback
        std::vector<core::ArgObject> args;
        REQUIRE(handler.resolveComponentId(model_layer, cid_a, args) == cid_a);
    }
}

TEST_CASE("TetGenLibHandler::resolveComponentId - total failure returns nullopt with clear semantics")
{
    // 选择器无效、fallback_component_id=-1：所有路径都失败，应返回 nullopt
    ModelLayer model_layer;
    const Index cid_a = addMeshComponent(model_layer, "compA");

    TetGenLibHandler handler;

    SECTION("No selection, no fallback → nullopt")
    {
        std::vector<core::ArgObject> args;
        auto resolved = handler.resolveComponentId(model_layer, -1, args);
        CHECK_FALSE(resolved.has_value());
    }

    SECTION("Null selection, no fallback → nullopt")
    {
        auto args = makeArgsWithSelector(nullptr);
        auto resolved = handler.resolveComponentId(model_layer, -1, args);
        CHECK_FALSE(resolved.has_value());
    }

    SECTION("Selection with all invalid gids, no fallback → nullopt")
    {
        auto sel = std::make_shared<Selection>();
        sel->type = ElementEnum::Vertex;
        sel->component_id = -1;
        sel->ids = { 99999998, 99999999 };
        auto args = makeArgsWithSelector(sel);
        auto resolved = handler.resolveComponentId(model_layer, -1, args);
        CHECK_FALSE(resolved.has_value());
    }
}
