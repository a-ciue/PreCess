#include "ComponentData.h"
#include "CreateCylinderHandler.h"
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
HandlerMetaData cylinderMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateCylinder";
    meta_data.display_name = "创建圆柱体";
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

TEST_CASE("CreateCylinder execute creates cylinder component by write target", "[CreateCylinderPlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new CreateCylinderHandler };
    REQUIRE(feature_system.registerHandler(cylinderMetaData(), std::move(handler)));

    // 写入目标 2：新建临时模型承载（默认参数为完整圆柱）
    REQUIRE(feature_system.setParameter("CreateCylinder", 9, core::ArgObject::create<ArgTypeEnum::Combo>(2)));
    const std::string create_message = std::any_cast<std::string>(feature_system.invoke("CreateCylinder"));
    const Index component_id = parseComponentId(create_message);
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
