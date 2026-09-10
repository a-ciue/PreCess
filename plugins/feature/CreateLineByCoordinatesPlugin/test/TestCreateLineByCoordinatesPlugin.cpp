#include "ComponentData.h"
#include "CreateLineByCoordinatesHandler.h"
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

#ifndef CREATE_LINE_BY_COORDINATES_PLUGIN_PATH
#define CREATE_LINE_BY_COORDINATES_PLUGIN_PATH ""
#endif

namespace {
HandlerMetaData lineMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateLineByCoordinates";
    meta_data.display_name = "创建直线边（坐标）";
    return meta_data;
}
}

TEST_CASE("CreateLineByCoordinatesPlugin dll registers into FeatureSystem via SystemPluginManager", "[CreateLineByCoordinatesPlugin]")
{
    int argc = 1;
    char arg0[] = "TestCreateLineByCoordinatesPlugin";
    char* argv[] = { arg0, nullptr };
    QCoreApplication app(argc, argv);

    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    SystemPluginManager plugin_manager;
    REQUIRE(plugin_manager.addSystemRegister(FeatureSystem::name, std::make_unique<FeatureSystemRegister>(feature_system)));

    // 走真实的 dll 加载链路：QPluginLoader + json 元数据 + 系统注册器
    REQUIRE(plugin_manager.registerPlugin(CREATE_LINE_BY_COORDINATES_PLUGIN_PATH));

    auto infos = feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos[0]->name == "CreateLineByCoordinates");
    REQUIRE(infos[0]->display_name == "创建直线边（坐标）");
    // 参数声明与原 GeometryOperationActions.qml 的 createLineByCoordinatesInfo 一致：6 个 Float + 1 个 Combo
    REQUIRE(infos[0]->arg_types.size() == 7);
    REQUIRE(infos[0]->menus.size() == 1);
    // 菜单与图标复用原"几何"页"直线边（坐标）"按钮的声明
    REQUIRE(infos[0]->menus[0].menu_path == "几何");
    REQUIRE(infos[0]->menus[0].icon == "qrc:/images/toolbar/Geometry/line_coord.svg");

    plugin_manager.unregisterPlugin(CREATE_LINE_BY_COORDINATES_PLUGIN_PATH);
    REQUIRE(feature_system.getFeatureInfos().empty());
}

TEST_CASE("CreateLineByCoordinates execute creates line component by write target", "[CreateLineByCoordinatesPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new CreateLineByCoordinatesHandler };
    REQUIRE(feature_system.registerHandler(lineMetaData(), std::move(handler)));

    // 默认写入目标 0（添加到当前 Component）且无活动组件：返回界面提示语，不产生组件
    const std::any hint = feature_system.invoke("CreateLineByCoordinates");
    REQUIRE(std::any_cast<std::string>(hint) == "请选择当前 Component，或修改写入目标。");

    // 写入目标 2：新建临时模型承载，返回新组件 id
    REQUIRE(feature_system.setParameter("CreateLineByCoordinates", 6, core::ArgObject::create<ArgTypeEnum::Combo>(2)));
    const Index component_id = std::any_cast<Index>(feature_system.invoke("CreateLineByCoordinates"));
    const auto* component = model_layer.findComponent(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->name == "Line_1");
    REQUIRE(component->geometry != nullptr);
    REQUIRE(component->geometry->rootShape != nullptr);
}
