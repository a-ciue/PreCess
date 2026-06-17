/**
 * @file TestIgesModelHandler.cpp
 * @brief IGES 模型文件处理器单元测试
 * @author PreCess Team
 */
#include "IgesModelHandler.h"
#include "ComponentData.h"
#include "GeometryData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelPayload.h"
#include "TempFile.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <TopoDS_Shape.hxx>
#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static ComponentDatas makeGeometryModel(
    const TopoDS_Shape& shape,
    const std::string& componentName)
{
    ComponentDatas comps;
    auto component = std::make_unique<ComponentData>();
    component->id = -1;
    component->name = componentName;

    auto geometry = std::make_unique<GeometryData>();
    geometry->rootShape = std::make_unique<TopoDS_Shape>(shape);
    component->geometry = std::move(geometry);

    comps.push_back(std::move(component));
    return comps;
}

static std::vector<Index> addModelAndGetComponentIds(
    ModelLayer& layer,
    ComponentDatas comps)
{
    const Index modelId = layer.addModel("model", std::move(comps));
    REQUIRE(modelId >= 0);

    std::vector<Index> componentIds = layer.modelById(modelId)->componentIds();
    REQUIRE(!componentIds.empty());

    return componentIds;
}

static void requireReadableGeometryModel(const ModelPayload& payload)
{
    REQUIRE(!payload.components.empty());

    bool hasValidGeometry = false;
    for (const auto& component : payload.components) {
        if (component
            && component->geometry
            && component->geometry->rootShape
            && !component->geometry->rootShape->IsNull()) {
            hasValidGeometry = true;
            break;
        }
    }

    REQUIRE(hasValidGeometry);
}

/**
 * @brief 创建测试：英文路径下立方体读写回环测试
 */
TEST_CASE("IgesModelHandler::write_components()/read_model() - English path")
{
    systems::io::IgesModelHandler io;
    ModelLayer layer;
    fs::path out = core::TempFile::instance().path().string() + ".igs";

    // 步骤1: 使用 OpenCASCADE 创建立方体
    TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0);
    REQUIRE(!box.IsNull());

    // 步骤2: 包装为组件化 GeometryData
    std::vector<Index> componentIds = addModelAndGetComponentIds(
        layer,
        makeGeometryModel(box, "Box"));

    // 步骤3: 写入 IGES 文件
    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    // 步骤4: 读取 IGES 文件
    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());

    // 步骤5: 验证读取到有效的组件化 Geometry 几何
    requireReadableGeometryModel(*payload);
}

/**
 * @brief 创建测试：中文文件名（仅文件名含中文）
 */
TEST_CASE("IgesModelHandler::write_components()/read_model() - Chinese filename")
{
    systems::io::IgesModelHandler io;
    ModelLayer layer;

    // 创建中文文件名
    fs::path out = core::TempFile::instance().path();
    out.replace_filename("测试_立方体_" + out.stem().string() + ".igs");

    // 步骤1: 使用 OpenCASCADE 创建立方体
    TopoDS_Shape box = BRepPrimAPI_MakeBox(15.0, 25.0, 35.0);
    REQUIRE(!box.IsNull());

    // 步骤2: 包装为组件化 GeometryData
    std::vector<Index> componentIds = addModelAndGetComponentIds(
        layer,
        makeGeometryModel(box, "中文立方体"));

    // 步骤3: 写入 IGES 文件
    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    // 步骤4: 读取 IGES 文件
    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());

    // 步骤5: 验证读取到有效的组件化 Geometry 几何
    requireReadableGeometryModel(*payload);
}

/**
 * @brief 创建测试：中文完整路径（目录+文件名均含中文）
 */
TEST_CASE("IgesModelHandler::write_components()/read_model() - Chinese full path")
{
    systems::io::IgesModelHandler io;
    ModelLayer layer;

    // 创建中文目录 + 中文文件名的完整路径
    fs::path out = core::TempFile::instance().path();
    out = out.parent_path() / "测试数据目录" / "子目录_模型" / ("测试_立方体_" + out.stem().string() + ".igs");
    fs::create_directories(out.parent_path());

    // 步骤1: 使用 OpenCASCADE 创建立方体
    TopoDS_Shape box = BRepPrimAPI_MakeBox(12.0, 22.0, 32.0);
    REQUIRE(!box.IsNull());

    // 步骤2: 包装为组件化 GeometryData
    std::vector<Index> componentIds = addModelAndGetComponentIds(
        layer,
        makeGeometryModel(box, "完整路径立方体"));

    // 步骤3: 写入 IGES 文件
    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    // 步骤4: 读取 IGES 文件
    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());

    // 步骤5: 验证读取到有效的组件化 Geometry 几何
    requireReadableGeometryModel(*payload);
}

/**
 * @brief 创建测试：球体读写回环测试
 */
TEST_CASE("IgesModelHandler::write_components()/read_model() - Sphere test")
{
    systems::io::IgesModelHandler io;
    ModelLayer layer;
    fs::path out = core::TempFile::instance().path().string() + "_sphere.igs";

    // 步骤1: 使用 OpenCASCADE 创建球体
    TopoDS_Shape sphere = BRepPrimAPI_MakeSphere(50.0);
    REQUIRE(!sphere.IsNull());

    // 步骤2: 包装为组件化 GeometryData
    std::vector<Index> componentIds = addModelAndGetComponentIds(
        layer,
        makeGeometryModel(sphere, "Sphere"));

    // 步骤3: 写入 IGES 文件
    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    // 步骤4: 读取 IGES 文件
    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());

    // 步骤5: 验证读取到有效的组件化 Geometry 几何
    requireReadableGeometryModel(*payload);
}

/**
 * @brief 测试：非 Geometry 模型写入（错误处理测试）
 */
TEST_CASE("IgesModelHandler::write_components() - Non-Geometry component handling")
{
    systems::io::IgesModelHandler io;
    ModelLayer layer;
    fs::path out = core::TempFile::instance().path().string() + "_non_geometry.igs";

    if (fs::exists(out)) {
        fs::remove(out);
    }

    // 创建一个非 Geometry 组件模型（只有 MeshData，没有 GeometryData）
    auto meshData = std::make_unique<MeshData>();
    ComponentDatas comps;
    auto comp = std::make_unique<ComponentData>();
    comp->id = -1;
    comp->mesh = std::move(meshData);
    comps.push_back(std::move(comp));
    std::vector<Index> componentIds = addModelAndGetComponentIds(layer, std::move(comps));

    // 应该不会抛出异常，但会记录错误日志，不写入文件
    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE_FALSE(fs::exists(out));
}
