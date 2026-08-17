/**
 * @file TestMeshRepairPlugin.cpp
 * @brief MeshRepair Feature 插件单元测试
 *
 * FeatureSystem 驱动模式：通过 Selector 参数显式传入目标 Component，验证
 *   1) 参数声明（Selector + Combo）
 *   2) 未选 Component 时的兜底提示
 *   3) 孔洞三角化填补的"已修补 M/N"成功计数报告
 *   4) 自交检测、退化清理的"未发现"提示
 *
 * 测试网格：单位立方体的 8 顶点 + 12 三角面（拆分四边形），用于验证 CGAL
 * 无退化、自交、孔洞场景。
 */

#include "ArgObject.h"
#include "ArgType.h"
#include "ComponentData.h"
#include "Core.h"
#include "EventBus.h"
#include "FeatureInfo.h"
#include "FeatureSystem.h"
#include "MeshData.h"
#include "MeshRepairHandler.h"
#include "ModelLayer.h"
#include "Selection.h"

#include <catch2/catch_test_macros.hpp>

#include <any>
#include <memory>
#include <string>
#include <vector>

using namespace systems;
using namespace systems::feature;

namespace {

/**
 * @brief 构造含 1 个三角孔洞的简单平面网格（4 顶点 / 2 三角面 / 1 三角形孔洞）
 *
 * 顶点布局（z=0 平面）：
 *   v0 = (0,0), v1 = (2,0), v2 = (2,2), v3 = (0,2)
 *
 * 现有 2 个三角面：左下 (v0,v1,v2) 与左上 (v0,v2,v3)，缺口 (v1,v2,v3) 形成
 * 1 个三角形孔洞；边界环上每个顶点恰好出现一次（manifold），PMP 可正常三角化。
 */
Index addHoleMeshComponent(ModelLayer& model_layer)
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = {
        { 0.0, 0.0, 0.0 }, // 0
        { 2.0, 0.0, 0.0 }, // 1
        { 2.0, 2.0, 0.0 }, // 2
        { 0.0, 2.0, 0.0 }, // 3
    };
    mesh->face_vertices_ = {
        // 左下三角
        0, 1, 2,
        // 左上三角
        0, 2, 3,
        // 缺口 (v1, v2, v3) 未三角化 → 单个三角形孔洞
    };
    mesh->face_vertices_offset_ = { 0, 3, 6 };

    auto component = std::make_unique<ComponentData>();
    component->name = "HoleMeshComponent";
    component->mesh = std::move(mesh);
    ComponentDatas components;
    components.push_back(std::move(component));

    const Index model_id = model_layer.addModel("HoleMeshModel", std::move(components));
    return model_layer.modelById(model_id)->componentIds().front();
}

/**
 * @brief 构造无孔洞、无自交、无退化的封闭盒面网格（12 三角面 → 立方体表面）
 */
Index addClosedCubeComponent(ModelLayer& model_layer)
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = {
        { 0.0, 0.0, 0.0 }, // 0
        { 1.0, 0.0, 0.0 }, // 1
        { 1.0, 1.0, 0.0 }, // 2
        { 0.0, 1.0, 0.0 }, // 3
        { 0.0, 0.0, 1.0 }, // 4
        { 1.0, 0.0, 1.0 }, // 5
        { 1.0, 1.0, 1.0 }, // 6
        { 0.0, 1.0, 1.0 }, // 7
    };
    // 12 个三角面，覆盖立方体 6 个面（每面拆为 2 三角）
    mesh->face_vertices_ = {
        // 底 z=0
        0, 2, 1,  0, 3, 2,
        // 顶 z=1
        4, 5, 6,  4, 6, 7,
        // 前 y=0
        0, 1, 5,  0, 5, 4,
        // 后 y=1
        3, 7, 6,  3, 6, 2,
        // 左 x=0
        0, 4, 7,  0, 7, 3,
        // 右 x=1
        1, 2, 6,  1, 6, 5,
    };
    mesh->face_vertices_offset_ = { 0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36 };

    auto component = std::make_unique<ComponentData>();
    component->name = "ClosedCubeComponent";
    component->mesh = std::move(mesh);
    ComponentDatas components;
    components.push_back(std::move(component));

    const Index model_id = model_layer.addModel("ClosedCubeModel", std::move(components));
    return model_layer.modelById(model_id)->componentIds().front();
}

/**
 * @brief 构造测试用的 MeshRepair HandlerMetaData
 */
HandlerMetaData repairMetaData()
{
    HandlerMetaData metadata;
    metadata.name = "MeshRepair";
    metadata.display_name = "网格修复";
    return metadata;
}

/**
 * @brief 创建指向目标 Component 的 Selector
 */
std::shared_ptr<Selection> makeComponentSelection(Index component_id)
{
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::Component;
    selection->ids = { component_id };
    return selection;
}

} // namespace

TEST_CASE("MeshRepair exposes Selector and Combo parameters", "[MeshRepairPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new MeshRepairHandler };
    REQUIRE(feature_system.registerHandler(repairMetaData(), std::move(handler)));

    const auto infos = feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos.front()->arg_types.size() == 2);

    // 参数 0：Selector 限定为 Component
    REQUIRE(infos.front()->arg_types.front().type == ArgTypeEnum::Selector);
    REQUIRE(infos.front()->arg_types.front().content == "Component");

    // 参数 1：Combo 列出三种操作
    REQUIRE(infos.front()->arg_types[1].type == ArgTypeEnum::Combo);
    REQUIRE(infos.front()->arg_types[1].content.find("补洞") != std::string::npos);
    REQUIRE(infos.front()->arg_types[1].content.find("自交检测") != std::string::npos);
    REQUIRE(infos.front()->arg_types[1].content.find("退化清理") != std::string::npos);
}

TEST_CASE("MeshRepair returns guidance when no Component is selected", "[MeshRepairPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new MeshRepairHandler };
    REQUIRE(feature_system.registerHandler(repairMetaData(), std::move(handler)));

    // 未设置任何 Selector：execute 应返回"请选择一个 Component"
    const std::any result = feature_system.invoke("MeshRepair");
    REQUIRE(result.type() == typeid(std::string));
    REQUIRE(std::any_cast<const std::string&>(result).find("请选择一个 Component") != std::string::npos);
}

TEST_CASE("MeshRepair fillHoles patches missing triangle hole", "[MeshRepairPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    const Index component_id = addHoleMeshComponent(model_layer);

    // 确认起点：2 个面，1 个三角形孔洞
    const ComponentData* before = model_layer.findComponent(component_id);
    REQUIRE(before != nullptr);
    REQUIRE(before->mesh != nullptr);
    const std::size_t initial_faces = before->mesh->face_vertices_offset_.size() - 1;
    REQUIRE(initial_faces == 2);

    FeatureSystem::SystemHandlerPtr handler { new MeshRepairHandler };
    REQUIRE(feature_system.registerHandler(repairMetaData(), std::move(handler)));
    REQUIRE(feature_system.setParameter(
        "MeshRepair", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeComponentSelection(component_id))));
    REQUIRE(feature_system.setParameter(
        "MeshRepair", 1,
        core::ArgObject::create<ArgTypeEnum::Combo>(0))); // 补洞

    const std::any result = feature_system.invoke("MeshRepair");
    REQUIRE(result.type() == typeid(std::string));
    const std::string text = std::any_cast<const std::string&>(result);
    REQUIRE(text.find("已修补 1/1") != std::string::npos);
    REQUIRE(text.find("网格已更新") != std::string::npos);

    // 补洞后面数 > 起始（CGAL 三角化可能产生 1~N 个面，取决于细化策略）
    const ComponentData* after = model_layer.findComponent(component_id);
    REQUIRE(after != nullptr);
    REQUIRE(after->mesh != nullptr);
    REQUIRE((after->mesh->face_vertices_offset_.size() - 1) > initial_faces);
}

TEST_CASE("MeshRepair detectSelfIntersections reports none on closed cube", "[MeshRepairPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    const Index component_id = addClosedCubeComponent(model_layer);

    FeatureSystem::SystemHandlerPtr handler { new MeshRepairHandler };
    REQUIRE(feature_system.registerHandler(repairMetaData(), std::move(handler)));
    REQUIRE(feature_system.setParameter(
        "MeshRepair", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeComponentSelection(component_id))));
    REQUIRE(feature_system.setParameter(
        "MeshRepair", 1,
        core::ArgObject::create<ArgTypeEnum::Combo>(1))); // 自交检测

    const std::any result = feature_system.invoke("MeshRepair");
    REQUIRE(result.type() == typeid(std::string));
    REQUIRE(std::any_cast<const std::string&>(result) == "未发现自相交面");
}

TEST_CASE("MeshRepair removeDegenerateFaces reports none on closed cube", "[MeshRepairPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    const Index component_id = addClosedCubeComponent(model_layer);

    FeatureSystem::SystemHandlerPtr handler { new MeshRepairHandler };
    REQUIRE(feature_system.registerHandler(repairMetaData(), std::move(handler)));
    REQUIRE(feature_system.setParameter(
        "MeshRepair", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeComponentSelection(component_id))));
    REQUIRE(feature_system.setParameter(
        "MeshRepair", 1,
        core::ArgObject::create<ArgTypeEnum::Combo>(2))); // 退化清理

    const std::any result = feature_system.invoke("MeshRepair");
    REQUIRE(result.type() == typeid(std::string));
    REQUIRE(std::any_cast<const std::string&>(result) == "未发现退化面");
}

/**
 * @brief 构造引用越界顶点的坏网格（面顶点 99 超出局部点索引范围）
 */
Index addBrokenMeshComponent(ModelLayer& model_layer)
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = {
        { 0.0, 0.0, 0.0 }, // 0
        { 1.0, 0.0, 0.0 }, // 1
        { 0.5, 1.0, 0.0 }, // 2
    };
    // 故意让面引用 99（远超出 3 个顶点的范围）
    mesh->face_vertices_ = { 0, 1, 99 };
    mesh->face_vertices_offset_ = { 0, 3 };

    auto component = std::make_unique<ComponentData>();
    component->name = "BrokenMeshComponent";
    component->mesh = std::move(mesh);
    ComponentDatas components;
    components.push_back(std::move(component));

    const Index model_id = model_layer.addModel("BrokenMeshModel", std::move(components));
    return model_layer.modelById(model_id)->componentIds().front();
}

TEST_CASE("MeshRepair converts CgalMeshAdapter exception to friendly text", "[MeshRepairPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    const Index component_id = addBrokenMeshComponent(model_layer);

    FeatureSystem::SystemHandlerPtr handler { new MeshRepairHandler };
    REQUIRE(feature_system.registerHandler(repairMetaData(), std::move(handler)));
    REQUIRE(feature_system.setParameter(
        "MeshRepair", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeComponentSelection(component_id))));
    REQUIRE(feature_system.setParameter(
        "MeshRepair", 1,
        core::ArgObject::create<ArgTypeEnum::Combo>(0))); // 补洞

    // 越界顶点 → toSurfaceMesh 抛 std::runtime_error → 应被 catch 转温和文案
    const std::any result = feature_system.invoke("MeshRepair");
    REQUIRE(result.type() == typeid(std::string));
    const std::string text = std::any_cast<const std::string&>(result);
    REQUIRE(text.find("网格修复失败") != std::string::npos);
    // 不应包含 CGAL 内部表达式
    REQUIRE(text.find("Expr:") == std::string::npos);
    REQUIRE(text.find("File:") == std::string::npos);
    REQUIRE(text.find("Line:") == std::string::npos);
}

/**
 * @brief 构造含 1 个四面体的网格：在封闭立方体表面的基础上叠加 1 个 solid 单元
 *
 * 表面仍是 12 三角面（满足 PMP 三角网格语义），但 solid_vertices_offset_ 标记 1 个体，
 * 触发入口预检 "含体单元"。
 */
Index addMeshWithSolidsComponent(ModelLayer& model_layer)
{
    const Index component_id = addClosedCubeComponent(model_layer);
    ComponentData* comp = model_layer.findComponent(component_id);
    REQUIRE(comp != nullptr);
    REQUIRE(comp->mesh != nullptr);

    // 用 4 顶点定义 1 个 VTK_TETRA (= 10) 体单元；预检只看 offset 长度，不校验拓扑
    comp->mesh->solid_types_.push_back(10);
    comp->mesh->solid_vertices_ = { 0, 1, 2, 3 };
    comp->mesh->solid_vertices_offset_ = { 0, 4 };
    return component_id;
}

/**
 * @brief 构造含 1 个四边形面的网格：在封闭立方体表面的基础上把某三角面合并为四边形
 *
 * 仍走"面选择"路径：6 个面有 1 个是 4 顶点（4 边形），其余 5 个三角面；
 * 触发入口预检 "含非三角面"。
 */
Index addMeshWithQuadFaceComponent(ModelLayer& model_layer)
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = {
        { 0.0, 0.0, 0.0 }, // 0
        { 1.0, 0.0, 0.0 }, // 1
        { 1.0, 1.0, 0.0 }, // 2
        { 0.0, 1.0, 0.0 }, // 3
    };
    // 1 个四边形面 (0,1,2,3) + 1 个三角面 (0,2,3)：故意非全三角，触发预检
    mesh->face_vertices_ = {
        0, 1, 2, 3, // 四边形
        0, 2, 3,    // 三角面
    };
    mesh->face_vertices_offset_ = { 0, 4, 7 };

    auto component = std::make_unique<ComponentData>();
    component->name = "QuadFaceComponent";
    component->mesh = std::move(mesh);
    ComponentDatas components;
    components.push_back(std::move(component));

    const Index model_id = model_layer.addModel("QuadFaceModel", std::move(components));
    return model_layer.modelById(model_id)->componentIds().front();
}

TEST_CASE("MeshRepair rejects components that contain solid cells", "[MeshRepairPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    const Index component_id = addMeshWithSolidsComponent(model_layer);

    FeatureSystem::SystemHandlerPtr handler { new MeshRepairHandler };
    REQUIRE(feature_system.registerHandler(repairMetaData(), std::move(handler)));
    REQUIRE(feature_system.setParameter(
        "MeshRepair", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeComponentSelection(component_id))));
    REQUIRE(feature_system.setParameter(
        "MeshRepair", 1,
        core::ArgObject::create<ArgTypeEnum::Combo>(0))); // 补洞（任意 op，预检在 op 之前）

    // 含体单元 → 入口预检直接返回，不进入 CGAL 路径
    const std::any result = feature_system.invoke("MeshRepair");
    REQUIRE(result.type() == typeid(std::string));
    const std::string expected
        = "网格修复仅支持纯三角表面网格（当前 Component 含体单元，请改用其他工具）";
    REQUIRE(std::any_cast<const std::string&>(result) == expected);

    // 兜底：网格数据应保持不变（无 CGAL 副作用）
    const ComponentData* after = model_layer.findComponent(component_id);
    REQUIRE(after != nullptr);
    REQUIRE(after->mesh != nullptr);
    REQUIRE(after->mesh->solid_vertices_offset_.size() == 2);
}

TEST_CASE("MeshRepair rejects components with non-triangular faces", "[MeshRepairPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    const Index component_id = addMeshWithQuadFaceComponent(model_layer);

    FeatureSystem::SystemHandlerPtr handler { new MeshRepairHandler };
    REQUIRE(feature_system.registerHandler(repairMetaData(), std::move(handler)));
    REQUIRE(feature_system.setParameter(
        "MeshRepair", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(makeComponentSelection(component_id))));
    REQUIRE(feature_system.setParameter(
        "MeshRepair", 1,
        core::ArgObject::create<ArgTypeEnum::Combo>(1))); // 自交检测（任意 op，预检在 op 之前）

    // 含非三角面 → 入口预检直接返回，不进入 CGAL 路径
    const std::any result = feature_system.invoke("MeshRepair");
    REQUIRE(result.type() == typeid(std::string));
    const std::string expected
        = "网格修复仅支持纯三角表面网格（当前 Component 含非三角面，请先转换为全三角网格）";
    REQUIRE(std::any_cast<const std::string&>(result) == expected);
}