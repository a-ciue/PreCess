/**
 * @file TestStepModelHandler.cpp
 * @brief STEP 模型文件处理器单元测试
 *
 * StepModelHandler 基于 OpenCASCADE 的 STEPControl_Reader/Writer，
 * 当前以组件化 GeometryData（封装 TopoDS_Shape）作为 Geometry 几何读写对象。
 * 测试模式参考 IGES 的测试：用 OCCT 在内存中构造 box/sphere，写出 STEP 再读回，
 * 验证读取到有效组件化几何；同时覆盖中文路径。
 */
#include "StepModelHandler.h"
#include "ComponentData.h"
#include "GeometryData.h"
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

TEST_CASE("StepModelHandler::write_components()/read_model() - English path (box)")
{
    systems::io::StepModelHandler io;
    ModelLayer layer;
    fs::path out = core::TempFile::instance().path().string() + ".stp";

    TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0);
    REQUIRE(!box.IsNull());

    std::vector<Index> componentIds = addModelAndGetComponentIds(
        layer,
        makeGeometryModel(box, "Box"));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());

    requireReadableGeometryModel(*payload);

    // model_name 应被设置为文件名
    REQUIRE(payload->model_name == out.filename().string());
}

TEST_CASE("StepModelHandler::write_components()/read_model() - Chinese filename")
{
    // 验证中文路径支持（path.u8string()）
    systems::io::StepModelHandler io;
    ModelLayer layer;

    fs::path out = core::TempFile::instance().path();
    out.replace_filename("中文_测试_" + out.stem().string() + ".stp");

    TopoDS_Shape box = BRepPrimAPI_MakeBox(15.0, 25.0, 35.0);
    REQUIRE(!box.IsNull());

    std::vector<Index> componentIds = addModelAndGetComponentIds(
        layer,
        makeGeometryModel(box, "中文立方体"));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());

    requireReadableGeometryModel(*payload);
}

TEST_CASE("StepModelHandler::write_components()/read_model() - Chinese full path")
{
    systems::io::StepModelHandler io;
    ModelLayer layer;

    fs::path out = core::TempFile::instance().path();
    out = out.parent_path() / "测试数据目录" / "子目录_模型"
        / ("中文_" + out.stem().string() + ".stp");
    fs::create_directories(out.parent_path());

    TopoDS_Shape box = BRepPrimAPI_MakeBox(12.0, 22.0, 32.0);
    REQUIRE(!box.IsNull());

    std::vector<Index> componentIds = addModelAndGetComponentIds(
        layer,
        makeGeometryModel(box, "完整路径立方体"));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());

    requireReadableGeometryModel(*payload);
}

TEST_CASE("StepModelHandler::write_components()/read_model() - sphere")
{
    systems::io::StepModelHandler io;
    ModelLayer layer;
    fs::path out = core::TempFile::instance().path().string() + "_sphere.stp";

    TopoDS_Shape sphere = BRepPrimAPI_MakeSphere(50.0);
    REQUIRE(!sphere.IsNull());

    std::vector<Index> componentIds = addModelAndGetComponentIds(
        layer,
        makeGeometryModel(sphere, "Sphere"));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());

    requireReadableGeometryModel(*payload);
}

TEST_CASE("StepModelHandler::read_model() - non-existent file")
{
    systems::io::StepModelHandler io;
    fs::path bad = core::TempFile::instance().path().string() + "_not_exist.stp";

    // 文件不存在，read_model 应返回 nullopt（而不是崩溃）
    std::optional<ModelPayload> model;
    REQUIRE_NOTHROW(model = io.read_model(bad, {}));
    REQUIRE(!model.has_value());
}
