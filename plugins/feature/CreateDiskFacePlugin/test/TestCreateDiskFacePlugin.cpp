#include "ComponentData.h"
#include "CreateDiskFaceHandler.h"
#include "EventBus.h"
#include "FeatureSystem.h"
#include "GeometryData.h"
#include "MeshData.h"
#include "ModelLayer.h"

#include <catch2/catch_test_macros.hpp>

#include <any>

using namespace systems;
using namespace systems::feature;

namespace {
HandlerMetaData diskMetaData()
{
    HandlerMetaData meta_data;
    meta_data.name = "CreateDiskFace";
    meta_data.display_name = "创建圆盘/扇形面";
    return meta_data;
}
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
