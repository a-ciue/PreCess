/**
 * @file TestTetGenLibHandler.cpp
 * @brief TetGenLibHandler 单元测试
 */
#include "TetGenLibHandler.h"

#include "ArgObject.h"
#include "ComponentData.h"
#include "MeshData.h"
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
 * @brief 构造只包含一个目标 Component 的选择器参数（与 pr_83 组件选择器风格一致）
 */
std::shared_ptr<Selection> makeComponentSelection(Index component_id)
{
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::Component;
    selection->ids = { component_id };
    return selection;
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

    SECTION("Parameter 0 (Selector): target component via component selection")
    {
        CHECK(args[0].type == ArgTypeEnum::Selector);
        CHECK(args[0].name == "目标 Component");
        CHECK(args[0].content == "Component");
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

TEST_CASE("TetGenLibHandler::resolveComponentId - Path A: Component selection resolves directly")
{
    // 用户在选择器"组件"模式下点选了具体 component：
    // 算法系统应直接使用 selection.ids 中的 component_id，不再回退到对象树 fallback_component_id。
    ModelLayer model_layer;
    const Index cid_a = addMeshComponent(model_layer, "compA");
    const Index cid_b = addMeshComponent(model_layer, "compB");
    REQUIRE(cid_a != cid_b);

    TetGenLibHandler handler;

    SECTION("Single component selection takes precedence over fallback")
    {
        // 即便 fallback 指向其他 component，选择器选中的 component 应优先生效
        auto args_a = makeArgsWithSelector(makeComponentSelection(cid_a));
        REQUIRE(handler.resolveComponentId(model_layer, cid_b, args_a) == cid_a);

        auto args_b = makeArgsWithSelector(makeComponentSelection(cid_b));
        REQUIRE(handler.resolveComponentId(model_layer, cid_a, args_b) == cid_b);
    }

    SECTION("Multiple component selection is rejected")
    {
        auto sel = std::make_shared<Selection>();
        sel->type = ElementEnum::Component;
        sel->ids = { cid_a, cid_b };
        auto args = makeArgsWithSelector(sel);
        // TetGen 输入须为整张 surface，多选 component 必然拒收
        auto resolved = handler.resolveComponentId(model_layer, cid_a, args);
        CHECK_FALSE(resolved.has_value());
    }

    SECTION("Empty component selection falls back to fallback_component_id")
    {
        auto sel = std::make_shared<Selection>();
        sel->type = ElementEnum::Component;
        auto args = makeArgsWithSelector(sel);
        REQUIRE(handler.resolveComponentId(model_layer, cid_a, args) == cid_a);
    }

    SECTION("Non-component selection type falls back to fallback_component_id")
    {
        auto sel = std::make_shared<Selection>();
        sel->type = ElementEnum::Vertex;
        sel->ids = { 0 };
        auto args = makeArgsWithSelector(sel);
        REQUIRE(handler.resolveComponentId(model_layer, cid_a, args) == cid_a);
    }
}

TEST_CASE("TetGenLibHandler::resolveComponentId - Path B: fallback to fallback_component_id")
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

    SECTION("Multiple component selection, no fallback → nullopt")
    {
        const Index cid_b = addMeshComponent(model_layer, "compB");
        auto sel = std::make_shared<Selection>();
        sel->type = ElementEnum::Component;
        sel->ids = { cid_a, cid_b };
        auto args = makeArgsWithSelector(sel);
        auto resolved = handler.resolveComponentId(model_layer, -1, args);
        CHECK_FALSE(resolved.has_value());
    }
}
