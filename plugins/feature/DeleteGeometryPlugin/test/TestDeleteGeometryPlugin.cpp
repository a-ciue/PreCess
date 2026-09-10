#include "ComponentData.h"
#include "DeleteGeometryHandler.h"
#include "EventBus.h"
#include "GeometryData.h"
#include "FeatureSystem.h"
#include "FeatureSystemRegister.h"
#include "GeometryBuilder.h"
#include "ModelOperator.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "Selection.h"
#include "SystemPluginManager.h"

#include <QCoreApplication>
#include <catch2/catch_test_macros.hpp>

#include <any>

using namespace systems;
using namespace systems::feature;

#ifndef DELETE_GEOMETRY_PLUGIN_PATH
#define DELETE_GEOMETRY_PLUGIN_PATH ""
#endif

namespace {
HandlerMetaData handlerMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "DeleteGeometry";
    meta_data.display_name = "删除几何";
    return meta_data;
}

//! @brief 临时模型承载一个长方体几何组件，并建好几何索引
Index addBoxGeometryComponent(ModelLayer& model_layer)
{
    const Index model_id = model_layer.addModel("temp_Fixture", {});
    auto model_operator = model_layer.getModelOperator(model_id);
    REQUIRE(model_operator.has_value());
    auto geometry = std::make_unique<GeometryData>();
    geometry->setRootShape(GeometryBuilder::makeBox(0.0, 0.0, 0.0, 10.0, 10.0, 10.0));
    auto component = std::make_unique<ComponentData>();
    component->name = "Fixture";
    component->geometry = std::move(geometry);
    const Index component_id = model_operator->addGeometryComponent(std::move(component));
    model_layer.findComponent(component_id)->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    return component_id;
}
}

TEST_CASE("DeleteGeometryPlugin dll registers into FeatureSystem via SystemPluginManager", "[DeleteGeometryPlugin]")
{
    int argc = 1;
    char arg0[] = "TestDeleteGeometryPlugin";
    char* argv[] = { arg0, nullptr };
    QCoreApplication app(argc, argv);

    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    SystemPluginManager plugin_manager;
    REQUIRE(plugin_manager.addSystemRegister(FeatureSystem::name, std::make_unique<FeatureSystemRegister>(feature_system)));

    // 走真实的 dll 加载链路：QPluginLoader + json 元数据 + 系统注册器
    REQUIRE(plugin_manager.registerPlugin(DELETE_GEOMETRY_PLUGIN_PATH));

    auto infos = feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos[0]->name == "DeleteGeometry");
    REQUIRE(infos[0]->display_name == "删除几何");
    // 参数声明与原 GeometryOperationActions.qml 的 deleteGeometryInfo 一致：1 个 Selector + 1 个 Bool
    REQUIRE(infos[0]->arg_types.size() == 2);
    REQUIRE(infos[0]->arg_types[0].type == ArgTypeEnum::Selector);
    REQUIRE(infos[0]->arg_types[1].type == ArgTypeEnum::Bool);
    // 菜单与图标复用原"几何"页"删除几何"按钮的声明
    REQUIRE(infos[0]->menus[0].menu_path == "几何");
    REQUIRE(infos[0]->menus[0].icon == "qrc:/images/toolbar/Geometry/delete_geometry.svg");

    plugin_manager.unregisterPlugin(DELETE_GEOMETRY_PLUGIN_PATH);
    REQUIRE(feature_system.getFeatureInfos().empty());
}

TEST_CASE("DeleteGeometry execute removes selected solid from component root", "[DeleteGeometryPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    const Index component_id = addBoxGeometryComponent(model_layer);
    FeatureSystem::SystemHandlerPtr handler { new DeleteGeometryHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));

    // 未选择目标几何时返回界面提示语
    const std::any hint = feature_system.invoke("DeleteGeometry");
    REQUIRE(std::any_cast<std::string>(hint) == "请选择一个几何形状。");

    // 选择长方体（顶层实体）删除，保留下级拓扑
    auto* component = model_layer.findComponent(component_id);
    const auto& solid_ids = component->geometry->index.solid_local_to_global;
    REQUIRE(solid_ids.size() == 1 + 1); // 局部 id 从 1 起，0 号为保留槽
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::GeometrySolid;
    selection->ids = { solid_ids[1] }; // 下标从 1 起，0 号为保留槽
    selection->component_id = component_id;
    REQUIRE(feature_system.setParameter("DeleteGeometry", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(selection)));
    REQUIRE(feature_system.setParameter("DeleteGeometry", 1,
        core::ArgObject::create<ArgTypeEnum::Bool>(false)));

    // 操作目标由所选形状反查，返回被更新组件 id
    const Index result_component_id = std::any_cast<Index>(feature_system.invoke("DeleteGeometry"));
    REQUIRE(result_component_id == component_id);

    // 顶层实体被移除，根形状保留（保留直接下级拓扑时不重建实体）
    REQUIRE(component->geometry != nullptr);
    component->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    REQUIRE(component->geometry->index.solid_local_to_global.size() == 0 + 1);
    // 下级拓扑保留：6 个面仍在
    REQUIRE(component->geometry->index.face_local_to_global.size() == 6 + 1);
}
