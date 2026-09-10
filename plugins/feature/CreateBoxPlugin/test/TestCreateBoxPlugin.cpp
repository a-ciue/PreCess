#include "ComponentData.h"
#include "CreateBoxHandler.h"
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

#ifndef CREATE_BOX_PLUGIN_PATH
#define CREATE_BOX_PLUGIN_PATH ""
#endif

namespace {
HandlerMetaData boxMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateBox";
    meta_data.display_name = "创建长方体";
    return meta_data;
}
}

TEST_CASE("CreateBoxPlugin dll registers into FeatureSystem via SystemPluginManager", "[CreateBoxPlugin]")
{
    int argc = 1;
    char arg0[] = "TestCreateBoxPlugin";
    char* argv[] = { arg0, nullptr };
    QCoreApplication app(argc, argv);

    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    SystemPluginManager plugin_manager;
    REQUIRE(plugin_manager.addSystemRegister(FeatureSystem::name, std::make_unique<FeatureSystemRegister>(feature_system)));

    // 走真实的 dll 加载链路：QPluginLoader + json 元数据 + 系统注册器
    REQUIRE(plugin_manager.registerPlugin(CREATE_BOX_PLUGIN_PATH));

    auto infos = feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos[0]->name == "CreateBox");
    REQUIRE(infos[0]->display_name == "创建长方体");
    // 参数声明与原 GeometryOperationActions.qml 的 createBoxInfo 一致：6 个 Float + 1 个 Combo
    REQUIRE(infos[0]->arg_types.size() == 7);
    REQUIRE(infos[0]->menus.size() == 1);
    // 菜单与图标复用原"几何"页"长方体"按钮的声明
    REQUIRE(infos[0]->menus[0].menu_path == "几何");
    REQUIRE(infos[0]->menus[0].icon == "qrc:/images/toolbar/Geometry/cuboid.svg");

    plugin_manager.unregisterPlugin(CREATE_BOX_PLUGIN_PATH);
    REQUIRE(feature_system.getFeatureInfos().empty());
}

TEST_CASE("CreateBox execute writes geometry per write target", "[CreateBoxPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    // 活动模型：单网格组件的普通模型，供"新建 Component"目标挂载几何组件
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    auto component = std::make_unique<ComponentData>();
    component->name = "Comp_0";
    component->mesh = std::move(mesh);
    ComponentDatas components;
    components.push_back(std::move(component));
    const Index model_id = model_layer.addModel("target_model", std::move(components));
    feature_system.setActiveModelProvider([model_id]() { return std::optional<Index> { model_id }; });

    FeatureSystem::SystemHandlerPtr handler { new CreateBoxHandler };
    REQUIRE(feature_system.registerHandler(boxMetaData(), std::move(handler)));

    // 默认写入目标 0（添加到当前 Component）且无活动组件：安全空转，不产生任何组件
    REQUIRE_NOTHROW(feature_system.invoke("CreateBox"));
    REQUIRE(model_layer.findComponent(model_id)->geometry == nullptr);

    // 写入目标 1：在活动模型下新建几何组件，新建时名称编号递增
    REQUIRE(feature_system.setParameter("CreateBox", 6, core::ArgObject::create<ArgTypeEnum::Combo>(1)));
    REQUIRE_NOTHROW(feature_system.invoke("CreateBox"));
    REQUIRE(model_layer.modelById(model_id)->componentIds().size() == 2);
    {
        const Index geometry_component_id = model_layer.modelById(model_id)->componentIds().back();
        const auto* geometry_component = model_layer.findComponent(geometry_component_id);
        REQUIRE(geometry_component != nullptr);
        REQUIRE(geometry_component->name == "Box_1");
        REQUIRE(geometry_component->geometry != nullptr);
        REQUIRE(geometry_component->geometry->rootShape != nullptr);
    }

    // 再次执行：继续在活动模型下新建组件
    REQUIRE_NOTHROW(feature_system.invoke("CreateBox"));
    REQUIRE(model_layer.modelById(model_id)->componentIds().size() == 3);

    // 写入目标 2：新建临时模型承载，execute 返回新组件 id
    REQUIRE(feature_system.setParameter("CreateBox", 6, core::ArgObject::create<ArgTypeEnum::Combo>(2)));
    const std::any result = feature_system.invoke("CreateBox");
    const Index temp_component_id = std::any_cast<Index>(result);
    REQUIRE(temp_component_id >= 0);
    const auto* temp_component = model_layer.findComponent(temp_component_id);
    REQUIRE(temp_component != nullptr);
    REQUIRE(temp_component->name == "Box_3");
    REQUIRE(temp_component->geometry != nullptr);
}
