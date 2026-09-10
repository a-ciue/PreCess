#include "ComponentData.h"
#include "CreateConeHandler.h"
#include "EventBus.h"
#include "FeatureSystem.h"
#include "FeatureSystemRegister.h"
#include "GeometryData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "SystemPluginManager.h"

#include <QCoreApplication>
#include <catch2/catch_test_macros.hpp>

#include <any>

using namespace systems;
using namespace systems::feature;

#ifndef CREATE_CONE_PLUGIN_PATH
#define CREATE_CONE_PLUGIN_PATH ""
#endif

namespace {
HandlerMetaData coneMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateCone";
    meta_data.display_name = "创建圆锥/圆台";
    return meta_data;
}
}

TEST_CASE("CreateConePlugin dll registers into FeatureSystem via SystemPluginManager", "[CreateConePlugin]")
{
    int argc = 1;
    char arg0[] = "TestCreateConePlugin";
    char* argv[] = { arg0, nullptr };
    QCoreApplication app(argc, argv);

    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    SystemPluginManager plugin_manager;
    REQUIRE(plugin_manager.addSystemRegister(FeatureSystem::name, std::make_unique<FeatureSystemRegister>(feature_system)));

    // 走真实的 dll 加载链路：QPluginLoader + json 元数据 + 系统注册器
    REQUIRE(plugin_manager.registerPlugin(CREATE_CONE_PLUGIN_PATH));

    auto infos = feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos[0]->name == "CreateCone");
    REQUIRE(infos[0]->display_name == "创建圆锥/圆台");
    // 参数声明与原 GeometryOperationActions.qml 的 createConeInfo 一致：10 个 Float + 1 个 Combo
    REQUIRE(infos[0]->arg_types.size() == 11);
    REQUIRE(infos[0]->menus.size() == 1);
    // 菜单与图标复用原"几何"页"圆锥/圆台"按钮的声明
    REQUIRE(infos[0]->menus[0].menu_path == "几何");
    REQUIRE(infos[0]->menus[0].icon == "qrc:/images/toolbar/Geometry/cone_or_conical_stage.svg");

    plugin_manager.unregisterPlugin(CREATE_CONE_PLUGIN_PATH);
    REQUIRE(feature_system.getFeatureInfos().empty());
}

TEST_CASE("CreateCone execute creates cone component by write target", "[CreateConePlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new CreateConeHandler };
    REQUIRE(feature_system.registerHandler(coneMetaData(), std::move(handler)));

    // 写入目标 2：新建临时模型承载（默认参数为圆台）
    REQUIRE(feature_system.setParameter("CreateCone", 10, core::ArgObject::create<ArgTypeEnum::Combo>(2)));
    const Index component_id = std::any_cast<Index>(feature_system.invoke("CreateCone"));
    const auto* component = model_layer.findComponent(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->name == "Cone_1");
    REQUIRE(component->geometry != nullptr);
    REQUIRE(component->geometry->rootShape != nullptr);

    // 圆台：3 个面（侧面 + 底面 + 顶面）
    component->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    REQUIRE(component->geometry->index.face_local_to_global.size() == 3 + 1); // 局部 id 从 1 起，0 号为保留槽
}
