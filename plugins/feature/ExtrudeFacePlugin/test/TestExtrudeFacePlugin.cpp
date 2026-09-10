#include "ComponentData.h"
#include "ExtrudeFaceHandler.h"
#include "EventBus.h"
#include "GeometryData.h"
#include "FeatureSystem.h"
#include "FeatureSystemRegister.h"
#include "GeometryBuilder.h"
#include "GeometryShapeWriter.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "Selection.h"
#include "SystemPluginManager.h"

#include <QCoreApplication>
#include <catch2/catch_test_macros.hpp>

#include <any>

using namespace systems;
using namespace systems::feature;

#ifndef EXTRUDE_FACE_PLUGIN_PATH
#define EXTRUDE_FACE_PLUGIN_PATH ""
#endif

namespace {
HandlerMetaData handlerMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "ExtrudeFace";
    meta_data.display_name = "拉伸面为实体";
    return meta_data;
}

//! @brief 临时模型承载一个长方体几何组件，并建好几何索引
Index addBoxGeometryComponent(ModelLayer& model_layer)
{
    const Index component_id = GeometryShapeWriter::writeShape(model_layer,
        GeometryShapeWriter::WriteTarget {}, "Fixture",
        GeometryBuilder::makeBox(0.0, 0.0, 0.0, 10.0, 10.0, 10.0));
    model_layer.findComponent(component_id)->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    return component_id;
}
}

TEST_CASE("ExtrudeFacePlugin dll registers into FeatureSystem via SystemPluginManager", "[ExtrudeFacePlugin]")
{
    int argc = 1;
    char arg0[] = "TestExtrudeFacePlugin";
    char* argv[] = { arg0, nullptr };
    QCoreApplication app(argc, argv);

    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    SystemPluginManager plugin_manager;
    REQUIRE(plugin_manager.addSystemRegister(FeatureSystem::name, std::make_unique<FeatureSystemRegister>(feature_system)));

    // 走真实的 dll 加载链路：QPluginLoader + json 元数据 + 系统注册器
    REQUIRE(plugin_manager.registerPlugin(EXTRUDE_FACE_PLUGIN_PATH));

    auto infos = feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos[0]->name == "ExtrudeFace");
    REQUIRE(infos[0]->display_name == "拉伸面为实体");
    // 参数声明与原 GeometryOperationActions.qml 的 extrudeFaceInfo 一致：1 个 Selector + 4 个 Float
    REQUIRE(infos[0]->arg_types.size() == 5);
    REQUIRE(infos[0]->arg_types[0].type == ArgTypeEnum::Selector);
    // 菜单与图标复用原"几何"页"拉伸面"按钮的声明
    REQUIRE(infos[0]->menus[0].menu_path == "几何");
    REQUIRE(infos[0]->menus[0].icon == "qrc:/images/toolbar/Geometry/stretched_surface.svg");

    plugin_manager.unregisterPlugin(EXTRUDE_FACE_PLUGIN_PATH);
    REQUIRE(feature_system.getFeatureInfos().empty());
}

TEST_CASE("ExtrudeFace execute appends extruded solid to source component", "[ExtrudeFacePlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    const Index component_id = addBoxGeometryComponent(model_layer);
    feature_system.setActiveComponentProvider([component_id]() { return std::optional<Index> { component_id }; });

    FeatureSystem::SystemHandlerPtr handler { new ExtrudeFaceHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));

    // 未选择截面时返回界面提示语
    const std::any hint = feature_system.invoke("ExtrudeFace");
    REQUIRE(std::any_cast<std::string>(hint) == "请选择一个几何面。");

    // 选择长方体顶面（沿 Z 正向拉伸，长度取默认 10）
    auto* component = model_layer.findComponent(component_id);
    const auto& face_ids = component->geometry->index.face_local_to_global;
    REQUIRE(face_ids.size() == 6 + 1); // 局部 id 从 1 起，0 号为保留槽
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::GeometryFace;
    selection->ids = { face_ids[1] }; // 下标从 1 起，0 号为保留槽
    selection->component_id = component_id;
    REQUIRE(feature_system.setParameter("ExtrudeFace", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(selection)));

    const Index result_component_id = std::any_cast<Index>(feature_system.invoke("ExtrudeFace"));
    REQUIRE(result_component_id == component_id); // 写回源组件

    // 源面保留，拉伸结果以截面副本追加（拓扑独立）：6 源面 + 6 棱柱面，2 个实体
    component->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    REQUIRE(component->geometry->index.solid_local_to_global.size() == 1 + 1 + 1);
    REQUIRE(component->geometry->index.face_local_to_global.size() == 12 + 1);
}
