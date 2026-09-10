#include "ComponentData.h"
#include "CreatePointHandler.h"
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

#ifndef CREATE_POINT_PLUGIN_PATH
#define CREATE_POINT_PLUGIN_PATH ""
#endif

namespace {
HandlerMetaData pointMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreatePoint";
    meta_data.display_name = "创建点";
    return meta_data;
}
}

TEST_CASE("CreatePointPlugin dll registers into FeatureSystem via SystemPluginManager", "[CreatePointPlugin]")
{
    int argc = 1;
    char arg0[] = "TestCreatePointPlugin";
    char* argv[] = { arg0, nullptr };
    QCoreApplication app(argc, argv);

    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    SystemPluginManager plugin_manager;
    REQUIRE(plugin_manager.addSystemRegister(FeatureSystem::name, std::make_unique<FeatureSystemRegister>(feature_system)));

    // 走真实的 dll 加载链路：QPluginLoader + json 元数据 + 系统注册器
    REQUIRE(plugin_manager.registerPlugin(CREATE_POINT_PLUGIN_PATH));

    auto infos = feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos[0]->name == "CreatePoint");
    REQUIRE(infos[0]->display_name == "创建点");
    // 参数声明与原 GeometryOperationActions.qml 的 createPointInfo 一致：3 个 Float + 1 个 Combo
    REQUIRE(infos[0]->arg_types.size() == 4);
    REQUIRE(infos[0]->menus.size() == 1);
    // 菜单与图标复用原"几何"页"点"按钮的声明
    REQUIRE(infos[0]->menus[0].menu_path == "几何");
    REQUIRE(infos[0]->menus[0].icon == "qrc:/images/toolbar/Geometry/point.svg");

    plugin_manager.unregisterPlugin(CREATE_POINT_PLUGIN_PATH);
    REQUIRE(feature_system.getFeatureInfos().empty());
}

TEST_CASE("CreatePoint execute creates point component by write target", "[CreatePointPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new CreatePointHandler };
    REQUIRE(feature_system.registerHandler(pointMetaData(), std::move(handler)));

    // 默认写入目标 0（添加到当前 Component）且无活动组件：返回界面提示语，不产生组件
    const std::any hint = feature_system.invoke("CreatePoint");
    REQUIRE(std::any_cast<std::string>(hint) == "请选择当前 Component，或修改写入目标。");

    // 写入目标 2：新建临时模型承载，返回新组件 id
    REQUIRE(feature_system.setParameter("CreatePoint", 3, core::ArgObject::create<ArgTypeEnum::Combo>(2)));
    const Index component_id = std::any_cast<Index>(feature_system.invoke("CreatePoint"));
    const auto* component = model_layer.findComponent(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->name == "Point_1");
    REQUIRE(component->geometry != nullptr);
    REQUIRE(component->geometry->rootShape != nullptr);

    // 再次执行：编号递增
    const Index second_id = std::any_cast<Index>(feature_system.invoke("CreatePoint"));
    REQUIRE(model_layer.findComponent(second_id)->name == "Point_2");
}
