#include "ComponentData.h"
#include "CreateBoxHandler.h"
#include "EventBus.h"
#include "FeatureSystem.h"
#include "GeometryData.h"
#include "MeshData.h"
#include "ModelLayer.h"

#include <catch2/catch_test_macros.hpp>

#include <any>
#include <string>

using namespace systems;
using namespace systems::feature;

namespace {
HandlerMetaData boxMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateBox";
    meta_data.display_name = "创建长方体";
    return meta_data;
}
}


namespace {
//! @brief 从「创建…成功（组件 N）」中解析组件 id，供测试定位新建组件。
Index parseComponentId(const std::string& message)
{
    REQUIRE(message.find("成功") != std::string::npos);
    const auto end = message.find_last_of("0123456789");
    REQUIRE(end != std::string::npos);
    auto start = end;
    while (start > 0 && message[start - 1] >= '0' && message[start - 1] <= '9')
        --start;
    return static_cast<Index>(std::stoll(message.substr(start, end - start + 1)));
}
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
    const std::string create_message = std::any_cast<std::string>(feature_system.invoke("CreateBox"));
    REQUIRE(create_message.find("创建长方体成功") == 0);
    const Index temp_component_id = parseComponentId(create_message);
    const auto* temp_component = model_layer.findComponent(temp_component_id);
    REQUIRE(temp_component != nullptr);
    REQUIRE(temp_component->name == "Box_3");
    REQUIRE(temp_component->geometry != nullptr);
}
