#include "ComponentData.h"
#include "CreateRectangleFaceHandler.h"
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
HandlerMetaData rectangleMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateRectangleFace";
    meta_data.display_name = "创建矩形面";
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

TEST_CASE("CreateRectangleFace execute creates face component by write target", "[CreateRectangleFacePlugin]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    FeatureSystem feature_system(model_layer, bus);

    FeatureSystem::SystemHandlerPtr handler { new CreateRectangleFaceHandler };
    REQUIRE(feature_system.registerHandler(rectangleMetaData(), std::move(handler)));

    // 写入目标 2：新建临时模型承载，返回新组件 id（默认参数 10x10 矩形）
    REQUIRE(feature_system.setParameter("CreateRectangleFace", 6, core::ArgObject::create<ArgTypeEnum::Combo>(2)));
    const std::string create_message = std::any_cast<std::string>(feature_system.invoke("CreateRectangleFace"));
    const Index component_id = parseComponentId(create_message);
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
