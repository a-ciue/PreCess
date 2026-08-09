#include "ComponentData.h"
#include "EventBus.h"
#include "FeatureDemoHandler.h"
#include "FeatureSystem.h"
#include "FeatureSystemRegister.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "SystemPluginManager.h"

#include <QCoreApplication>
#include <catch2/catch_test_macros.hpp>

#include <array>

using namespace systems;
using namespace systems::feature;

#ifndef FEATURE_DEMO_PLUGIN_PATH
#define FEATURE_DEMO_PLUGIN_PATH ""
#endif

namespace {
HandlerMetaData demoMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "FeatureDemo";
    meta_data.display_name = "功能示例";
    return meta_data;
}

Index addSingleComponentModel(ModelLayer& model_layer)
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = { { 1.0, 2.0, 3.0 } };

    auto component = std::make_unique<ComponentData>();
    component->id = -1;
    component->name = "Comp_0";
    component->mesh = std::move(mesh);
    ComponentDatas components;
    components.push_back(std::move(component));

    Index model_id = model_layer.addModel("test_model", std::move(components));
    return model_layer.modelById(model_id)->componentIds().front();
}
}

TEST_CASE("FeatureDemoPlugin dll registers into FeatureSystem via SystemPluginManager", "[FeatureDemoPlugin]")
{
    int argc = 1;
    char arg0[] = "TestFeatureDemoPlugin";
    char* argv[] = { arg0, nullptr };
    QCoreApplication app(argc, argv);

    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    SystemPluginManager plugin_manager;
    REQUIRE(plugin_manager.addSystemRegister(FeatureSystem::name, std::make_unique<FeatureSystemRegister>(feature_system)));

    // 走真实的 dll 加载链路：QPluginLoader + json 元数据 + 系统注册器
    REQUIRE(plugin_manager.registerPlugin(FEATURE_DEMO_PLUGIN_PATH));

    auto infos = feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos[0]->name == "FeatureDemo");
    REQUIRE(infos[0]->display_name == "功能示例");
    REQUIRE(infos[0]->arg_types.size() == 2);
    REQUIRE(infos[0]->menus.size() == 3);
    // 第二个菜单贡献项声明了自定义图标
    REQUIRE(infos[0]->menus[1].menu_path == "功能/测量");
    REQUIRE(infos[0]->menus[1].icon == "qrc:/images/toolbar/Algorithm/gmsh.svg");
    REQUIRE(infos[0]->key_bindings.size() == 1);

    // 无活动组件时执行，功能应安全地空转
    REQUIRE_NOTHROW(feature_system.invoke("FeatureDemo"));

    REQUIRE(feature_system.setParameter("FeatureDemo", 1, core::ArgObject::create<ArgTypeEnum::Bool>(true)));

    plugin_manager.unregisterPlugin(FEATURE_DEMO_PLUGIN_PATH);
    REQUIRE(feature_system.getFeatureInfos().empty());
}

TEST_CASE("FeatureDemo execute scales active component mesh via context", "[FeatureDemoPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    const Index component_id = addSingleComponentModel(model_layer);

    FeatureSystem::SystemHandlerPtr handler { new FeatureDemoHandler };
    REQUIRE(feature_system.registerHandler(demoMetaData(), std::move(handler)));

    // 注入动态上下文：活动组件 id 由 UI 层（此处为测试）提供
    feature_system.setActiveComponentProvider([component_id]() { return std::optional<Index> { component_id }; });
    REQUIRE(feature_system.setParameter("FeatureDemo", 0, core::ArgObject::create<ArgTypeEnum::Float>(2.0)));
    REQUIRE_NOTHROW(feature_system.invoke("FeatureDemo"));

    const auto* component = model_layer.findComponent(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->mesh != nullptr);
    // 顶点坐标常驻组件 MeshData，直接验证就地缩放结果
    REQUIRE(component->mesh->vertex_positions_.size() == 1);
    REQUIRE(component->mesh->vertex_positions_[0] == std::array<double, 3> { 2.0, 4.0, 6.0 });
}
