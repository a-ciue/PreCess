#include "ComponentData.h"
#include "CreateDiskFaceHandler.h"
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

#ifndef CREATE_DISK_FACE_PLUGIN_PATH
#define CREATE_DISK_FACE_PLUGIN_PATH ""
#endif

namespace {
HandlerMetaData diskMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateDiskFace";
    meta_data.display_name = "创建圆盘/扇形面";
    return meta_data;
}
}

TEST_CASE("CreateDiskFacePlugin dll registers into FeatureSystem via SystemPluginManager", "[CreateDiskFacePlugin]")
{
    int argc = 1;
    char arg0[] = "TestCreateDiskFacePlugin";
    char* argv[] = { arg0, nullptr };
    QCoreApplication app(argc, argv);

    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    SystemPluginManager plugin_manager;
    REQUIRE(plugin_manager.addSystemRegister(FeatureSystem::name, std::make_unique<FeatureSystemRegister>(feature_system)));

    // 走真实的 dll 加载链路：QPluginLoader + json 元数据 + 系统注册器
    REQUIRE(plugin_manager.registerPlugin(CREATE_DISK_FACE_PLUGIN_PATH));

    auto infos = feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos[0]->name == "CreateDiskFace");
    REQUIRE(infos[0]->display_name == "创建圆盘/扇形面");
    // 参数声明与原 GeometryOperationActions.qml 的 createDiskFaceInfo 一致：6 个 Float + 2 个 Combo
    REQUIRE(infos[0]->arg_types.size() == 8);
    REQUIRE(infos[0]->menus.size() == 1);
    // 菜单与图标复用原"几何"页"圆盘/扇形面"按钮的声明
    REQUIRE(infos[0]->menus[0].menu_path == "几何");
    REQUIRE(infos[0]->menus[0].icon == "qrc:/images/toolbar/Geometry/sector_or_circle.svg");

    plugin_manager.unregisterPlugin(CREATE_DISK_FACE_PLUGIN_PATH);
    REQUIRE(feature_system.getFeatureInfos().empty());
}

TEST_CASE("CreateDiskFace execute creates disk component by write target", "[CreateDiskFacePlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new CreateDiskFaceHandler };
    REQUIRE(feature_system.registerHandler(diskMetaData(), std::move(handler)));

    // 写入目标 2：新建临时模型承载（默认参数为完整圆盘）
    REQUIRE(feature_system.setParameter("CreateDiskFace", 7, core::ArgObject::create<ArgTypeEnum::Combo>(2)));
    const Index component_id = std::any_cast<Index>(feature_system.invoke("CreateDiskFace"));
    const auto* component = model_layer.findComponent(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->name == "DiskFace_1");
    REQUIRE(component->geometry != nullptr);
    REQUIRE(component->geometry->rootShape != nullptr);

    // 完整圆盘：1 个面
    component->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    REQUIRE(component->geometry->index.face_local_to_global.size() == 1 + 1); // 局部 id 从 1 起，0 号为保留槽
}
