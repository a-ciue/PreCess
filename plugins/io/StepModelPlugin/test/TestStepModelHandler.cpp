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

#include <BRepBuilderAPI_Copy.hxx>
#include <BRepGProp.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepTools.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Iterator.hxx>
#include <TopoDS_Shape.hxx>
#include <catch2/catch_approx.hpp>
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
            REQUIRE(component->geometry->rootShape->ShapeType() == TopAbs_COMPOUND);
            for (TopoDS_Iterator it(*component->geometry->rootShape); it.More(); it.Next())
                REQUIRE(it.Value().ShapeType() != TopAbs_COMPOUND);
            hasValidGeometry = true;
            break;
        }
    }

    REQUIRE(hasValidGeometry);
}

TEST_CASE("CAD export topology copies share geometry and isolate BRep discretization")
{
    const TopoDS_Shape source = BRepPrimAPI_MakeSphere(10.0);
    BRepMesh_IncrementalMesh mesh(source, 0.2);
    REQUIRE(mesh.IsDone());
    const TopoDS_Shape copy = BRepBuilderAPI_Copy(source, false, false).Shape();
    CHECK_FALSE(copy.IsSame(source));
    TopExp_Explorer source_faces(source, TopAbs_FACE), copy_faces(copy, TopAbs_FACE);
    REQUIRE(source_faces.More());
    REQUIRE(copy_faces.More());
    const auto source_face = TopoDS::Face(source_faces.Current());
    const auto copy_face = TopoDS::Face(copy_faces.Current());
    CHECK_FALSE(copy_face.IsSame(source_face));
    TopLoc_Location source_location, copy_location;
    CHECK(BRep_Tool::Surface(source_face, source_location) == BRep_Tool::Surface(copy_face, copy_location));
    const auto source_mesh = BRep_Tool::Triangulation(source_face, source_location);
    REQUIRE_FALSE(source_mesh.IsNull());
    CHECK(BRep_Tool::Triangulation(copy_face, copy_location).IsNull());
    TopExp_Explorer source_edges(source_face, TopAbs_EDGE), copy_edges(copy_face, TopAbs_EDGE);
    while (source_edges.More() && BRep_Tool::Degenerated(TopoDS::Edge(source_edges.Current()))) {
        REQUIRE(copy_edges.More());
        source_edges.Next();
        copy_edges.Next();
    }
    REQUIRE(source_edges.More());
    REQUIRE(copy_edges.More());
    const auto source_edge = TopoDS::Edge(source_edges.Current());
    const auto copy_edge = TopoDS::Edge(copy_edges.Current());
    CHECK_FALSE(copy_edge.IsSame(source_edge));
    double source_first, source_last, copy_first, copy_last;
    REQUIRE_FALSE(BRep_Tool::Curve(source_edge, source_location, source_first, source_last).IsNull());
    CHECK(BRep_Tool::Curve(source_edge, source_location, source_first, source_last)
        == BRep_Tool::Curve(copy_edge, copy_location, copy_first, copy_last));
    CHECK(BRep_Tool::CurveOnSurface(source_edge, source_face, source_first, source_last)
        == BRep_Tool::CurveOnSurface(copy_edge, copy_face, copy_first, copy_last));
    CHECK(source_first == copy_first);
    CHECK(source_last == copy_last);
    BRepMesh_IncrementalMesh copy_mesh(copy, 0.5);
    REQUIRE(copy_mesh.IsDone());
    CHECK_FALSE(BRep_Tool::Triangulation(copy_face, copy_location).IsNull());
    CHECK(BRep_Tool::Triangulation(copy_face, copy_location) != source_mesh);
    BRepTools::Clean(copy);
    CHECK(BRep_Tool::Triangulation(copy_face, copy_location).IsNull());
    CHECK(BRep_Tool::Triangulation(source_face, source_location) == source_mesh);
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
    REQUIRE(payload->model_name == out.filename().u8string());
}

TEST_CASE("StepModelHandler::write_components()/read_model() - Chinese filename")
{
    // 验证中文路径支持（pathUtf8()）
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
    GProp_GProps expected;
    BRepGProp::VolumeProperties(sphere, expected);
    BRepMesh_IncrementalMesh mesh(sphere, 0.5);
    const auto face = TopoDS::Face(TopExp_Explorer(sphere, TopAbs_FACE).Current());
    TopLoc_Location location;
    const auto triangulation = BRep_Tool::Triangulation(face, location);
    REQUIRE_FALSE(triangulation.IsNull());

    std::vector<Index> componentIds = addModelAndGetComponentIds(
        layer,
        makeGeometryModel(sphere, "Sphere"));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    CHECK(BRep_Tool::Triangulation(face, location) == triangulation);
    GProp_GProps source_after;
    BRepGProp::VolumeProperties(sphere, source_after);
    CHECK(source_after.Mass() == Catch::Approx(expected.Mass()).epsilon(1e-12));

    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());

    requireReadableGeometryModel(*payload);

    double volume = 0.0;
    for (const auto& component : payload->components) {
        GProp_GProps actual;
        BRepGProp::VolumeProperties(*component->geometry->rootShape, actual);
        volume += actual.Mass();
    }
    CHECK(volume == Catch::Approx(expected.Mass()).epsilon(1e-6));
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
