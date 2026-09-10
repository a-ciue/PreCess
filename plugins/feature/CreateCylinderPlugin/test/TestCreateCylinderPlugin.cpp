#include "ComponentData.h"
#include "CreateCylinderHandler.h"
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

#ifndef CREATE_CYLINDER_PLUGIN_PATH
#define CREATE_CYLINDER_PLUGIN_PATH ""
#endif

namespace {
HandlerMetaData cylinderMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateCylinder";
    meta_data.display_name = "创建圆柱体";
    return meta_data;
}
}

TEST_CASE("CreateCylinderPlugin dll registers into FeatureSystem via SystemPluginManager", "[CreateCylinderPlugin]")
{
    int argc = 1;
    char arg0[] = "TestCreateCylinderPlugin";
    char* argv[] = { arg0, nullptr };
    QCoreApplication app(argc, argv);

    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    SystemPluginManager plugin_manager;
    REQUIRE(plugin_manager.addSystemRegister(FeatureSystem::name, std::make_unique<FeatureSystemRegister>(feature_system)));

    // 走真实的 dll 加载链路：QPluginLoader + json 元数据 + 系统注册器
    REQUIRE(plugin_manager.registerPlugin(CREATE_CYLINDER_PLUGIN_PATH));

    auto infos = feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos[0]->name == "CreateCylinder");
    REQUIRE(infos[0]->display_name == "创建圆柱体");
    // 参数声明与原 GeometryOperationActions.qml 的 createCylinderInfo 一致：9 个 Float + 1 个 Combo
    REQUIRE(infos[0]->arg_types.size() == 10);
    REQUIRE(infos[0]->menus.size() == 1);
    // 菜单与图标复用原"几何"页"圆柱体"按钮的声明
    REQUIRE(infos[0]->menus[0].menu_path == "几何");
    REQUIRE(infos[0]->menus[0].icon == "qrc:/images/toolbar/Geometry/cylinder.svg");

    plugin_manager.unregisterPlugin(CREATE_CYLINDER_PLUGIN_PATH);
    REQUIRE(feature_system.getFeatureInfos().empty());
}

TEST_CASE("CreateCylinder execute creates cylinder component by write target", "[CreateCylinderPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new CreateCylinderHandler };
    REQUIRE(feature_system.registerHandler(cylinderMetaData(), std::move(handler)));

    // 写入目标 2：新建临时模型承载（默认参数为完整圆柱）
    REQUIRE(feature_system.setParameter("CreateCylinder", 9, core::ArgObject::create<ArgTypeEnum::Combo>(2)));
    const Index component_id = std::any_cast<Index>(feature_system.invoke("CreateCylinder"));
    const auto* component = model_layer.findComponent(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->name == "Cylinder_1");
    REQUIRE(component->geometry != nullptr);
    REQUIRE(component->geometry->rootShape != nullptr);

    // 完整圆柱：2 个面（侧面 + 底面合环）与 2 个顶点
    component->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    REQUIRE(component->geometry->index.face_local_to_global.size() == 3 + 1); // 侧面 + 两底面；局部 id 从 1 起
    REQUIRE(component->geometry->index.vertex_local_to_global.size() == 2 + 1);
}
