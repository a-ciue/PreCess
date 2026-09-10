#include "ComponentData.h"
#include "CreateLineFromVerticesHandler.h"
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

#ifndef CREATE_LINE_FROM_VERTICES_PLUGIN_PATH
#define CREATE_LINE_FROM_VERTICES_PLUGIN_PATH ""
#endif

namespace {
HandlerMetaData handlerMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateLineFromVertices";
    meta_data.display_name = "创建直线边（选择两点）";
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

TEST_CASE("CreateLineFromVerticesPlugin dll registers into FeatureSystem via SystemPluginManager", "[CreateLineFromVerticesPlugin]")
{
    int argc = 1;
    char arg0[] = "TestCreateLineFromVerticesPlugin";
    char* argv[] = { arg0, nullptr };
    QCoreApplication app(argc, argv);

    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);
    SystemPluginManager plugin_manager;
    REQUIRE(plugin_manager.addSystemRegister(FeatureSystem::name, std::make_unique<FeatureSystemRegister>(feature_system)));

    // 走真实的 dll 加载链路：QPluginLoader + json 元数据 + 系统注册器
    REQUIRE(plugin_manager.registerPlugin(CREATE_LINE_FROM_VERTICES_PLUGIN_PATH));

    auto infos = feature_system.getFeatureInfos();
    REQUIRE(infos.size() == 1);
    REQUIRE(infos[0]->name == "CreateLineFromVertices");
    REQUIRE(infos[0]->display_name == "创建直线边（选择两点）");
    REQUIRE(infos[0]->arg_types.size() == 1);
    REQUIRE(infos[0]->arg_types[0].type == ArgTypeEnum::Selector);
    // 菜单与图标复用原"几何"页"直线边（选点）"按钮的声明
    REQUIRE(infos[0]->menus[0].menu_path == "几何");
    REQUIRE(infos[0]->menus[0].icon == "qrc:/images/toolbar/Geometry/line_points.svg");

    plugin_manager.unregisterPlugin(CREATE_LINE_FROM_VERTICES_PLUGIN_PATH);
    REQUIRE(feature_system.getFeatureInfos().empty());
}

TEST_CASE("CreateLineFromVertices execute appends shared-topology line to source component", "[CreateLineFromVerticesPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    const Index component_id = addBoxGeometryComponent(model_layer);
    feature_system.setActiveComponentProvider([component_id]() { return std::optional<Index> { component_id }; });

    FeatureSystem::SystemHandlerPtr handler { new CreateLineFromVerticesHandler };
    REQUIRE(feature_system.registerHandler(handlerMetaData(), std::move(handler)));

    // 未选择端点时返回界面提示语
    const std::any hint = feature_system.invoke("CreateLineFromVertices");
    REQUIRE(std::any_cast<std::string>(hint) == "请选择两个几何点。");

    // 选择长方体的两个顶点创建直线边
    auto* component = model_layer.findComponent(component_id);
    const auto& vertex_ids = component->geometry->index.vertex_local_to_global;
    REQUIRE(vertex_ids.size() == 8 + 1); // 局部 id 从 1 起，0 号为保留槽
    auto selection = std::make_shared<Selection>();
    selection->type = ElementEnum::GeometryVertex;
    selection->ids = { vertex_ids[1], vertex_ids[2] }; // 下标从 1 起，0 号为保留槽
    selection->component_id = component_id;
    REQUIRE(feature_system.setParameter("CreateLineFromVertices", 0,
        core::ArgObject::create<ArgTypeEnum::Selector>(selection)));

    const Index result_component_id = std::any_cast<Index>(feature_system.invoke("CreateLineFromVertices"));
    REQUIRE(result_component_id == component_id); // 写回源组件

    // 直线边共享既有顶点：边数 12 + 1
    component->geometry->ensureIndexBuilt(model_layer.geomRegistry());
    REQUIRE(component->geometry->index.edge_local_to_global.size() == 12 + 1 + 1);
}
