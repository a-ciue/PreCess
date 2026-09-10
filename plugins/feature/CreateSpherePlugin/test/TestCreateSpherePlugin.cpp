#include "ComponentData.h"
#include "CreateSphereHandler.h"
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

#ifndef CREATE_SPHERE_PLUGIN_PATH
#define CREATE_SPHERE_PLUGIN_PATH ""
#endif

namespace {
HandlerMetaData sphereMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateSphere";
    meta_data.display_name = "创建球体/部分球体";
    return meta_data;
}
}

TEST_CASE("CreateSpherePlugin dll registers into FeatureSystem via SystemPluginManager", "[CreateSpherePlugin]")
{
    int argc = 1;
    char arg0[] = "TestCreateSpherePlugin";
    char* argv[] = { arg0, nullptr };
    QCoreApplication app(argc, argv);

    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    SystemPluginManager plugin_manager;
    REQUIRE(plugin_manager.addSystemRegister(FeatureSystem::name, std::make_unique<FeatureSystemRegister>(feature_system)));

    // 走真实的 dll 加载链路：QPluginLoader + json 元数据 + 系统注册器
    REQUIRE(plugin_manager.registerPlugin(CREATE_SPHERE_PLUGIN_PATH));

    auto infos = feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos[0]->name == "CreateSphere");
    REQUIRE(infos[0]->display_name == "创建球体/部分球体");
    // 参数声明与原 GeometryOperationActions.qml 的 createSphereInfo 一致：10 个 Float + 1 个 Combo
    REQUIRE(infos[0]->arg_types.size() == 11);
    REQUIRE(infos[0]->menus.size() == 1);
    // 菜单与图标复用原"几何"页"球体/部分球体"按钮的声明
    REQUIRE(infos[0]->menus[0].menu_path == "几何");
    REQUIRE(infos[0]->menus[0].icon == "qrc:/images/toolbar/Geometry/sphere.svg");

    plugin_manager.unregisterPlugin(CREATE_SPHERE_PLUGIN_PATH);
    REQUIRE(feature_system.getFeatureInfos().empty());
}

TEST_CASE("CreateSphere execute creates sphere component by write target", "[CreateSpherePlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new CreateSphereHandler };
    REQUIRE(feature_system.registerHandler(sphereMetaData(), std::move(handler)));

    // 写入目标 2：新建临时模型承载（默认参数为完整球体）
    REQUIRE(feature_system.setParameter("CreateSphere", 10, core::ArgObject::create<ArgTypeEnum::Combo>(2)));
    const Index component_id = std::any_cast<Index>(feature_system.invoke("CreateSphere"));
    const auto* component = model_layer.findComponent(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->name == "Sphere_1");
    REQUIRE(component->geometry != nullptr);
    REQUIRE(component->geometry->rootShape != nullptr);

    // 完整球体：1 个面、无边、无独立顶点
    component->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    REQUIRE(component->geometry->index.face_local_to_global.size() == 1 + 1); // 局部 id 从 1 起，0 号为保留槽
    REQUIRE(component->geometry->index.edge_local_to_global.size() == 3 + 1); // 球面含缝合边与两极圈
    REQUIRE(component->geometry->index.vertex_local_to_global.size() == 2 + 1); // 两个极点
}
