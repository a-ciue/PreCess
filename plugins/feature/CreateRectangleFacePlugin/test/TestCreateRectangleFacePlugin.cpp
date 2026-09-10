#include "ComponentData.h"
#include "CreateRectangleFaceHandler.h"
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

#ifndef CREATE_RECTANGLE_FACE_PLUGIN_PATH
#define CREATE_RECTANGLE_FACE_PLUGIN_PATH ""
#endif

namespace {
HandlerMetaData rectangleMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateRectangleFace";
    meta_data.display_name = "创建矩形面";
    return meta_data;
}
}

TEST_CASE("CreateRectangleFacePlugin dll registers into FeatureSystem via SystemPluginManager", "[CreateRectangleFacePlugin]")
{
    int argc = 1;
    char arg0[] = "TestCreateRectangleFacePlugin";
    char* argv[] = { arg0, nullptr };
    QCoreApplication app(argc, argv);

    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    SystemPluginManager plugin_manager;
    REQUIRE(plugin_manager.addSystemRegister(FeatureSystem::name, std::make_unique<FeatureSystemRegister>(feature_system)));

    // 走真实的 dll 加载链路：QPluginLoader + json 元数据 + 系统注册器
    REQUIRE(plugin_manager.registerPlugin(CREATE_RECTANGLE_FACE_PLUGIN_PATH));

    auto infos = feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos[0]->name == "CreateRectangleFace");
    REQUIRE(infos[0]->display_name == "创建矩形面");
    // 参数声明与原 GeometryOperationActions.qml 的 createRectangleFaceInfo 一致：5 个 Float + 2 个 Combo
    REQUIRE(infos[0]->arg_types.size() == 7);
    REQUIRE(infos[0]->menus.size() == 1);
    // 菜单与图标复用原"几何"页"矩形面"按钮的声明
    REQUIRE(infos[0]->menus[0].menu_path == "几何");
    REQUIRE(infos[0]->menus[0].icon == "qrc:/images/toolbar/Geometry/rectangle.svg");

    plugin_manager.unregisterPlugin(CREATE_RECTANGLE_FACE_PLUGIN_PATH);
    REQUIRE(feature_system.getFeatureInfos().empty());
}

TEST_CASE("CreateRectangleFace execute creates face component by write target", "[CreateRectangleFacePlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new CreateRectangleFaceHandler };
    REQUIRE(feature_system.registerHandler(rectangleMetaData(), std::move(handler)));

    // 写入目标 2：新建临时模型承载，返回新组件 id（默认参数 10x10 矩形）
    REQUIRE(feature_system.setParameter("CreateRectangleFace", 6, core::ArgObject::create<ArgTypeEnum::Combo>(2)));
    const Index component_id = std::any_cast<Index>(feature_system.invoke("CreateRectangleFace"));
    const auto* component = model_layer.findComponent(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->name == "RectangleFace_1");
    REQUIRE(component->geometry != nullptr);
    REQUIRE(component->geometry->rootShape != nullptr);

    // 索引建成后应含 1 个面、4 条边、4 个顶点（共享拓扑）
    component->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    const auto& index = component->geometry->index;
    REQUIRE(index.face_local_to_global.size() == 1 + 1); // 局部 id 从 1 起，0 号为保留槽
    REQUIRE(index.edge_local_to_global.size() == 4 + 1);
    REQUIRE(index.vertex_local_to_global.size() == 4 + 1);
}
