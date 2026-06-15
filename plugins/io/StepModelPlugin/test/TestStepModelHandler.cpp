/**
 * @file TestStepModelHandler.cpp
 * @brief STEP 模型文件处理器单元测试
 *
 * StepModelHandler 基于 OpenCASCADE 的 STEPControl_Reader/Writer，
 * 处理对象是 SplineData（封装 TopoDS_Shape）。
 * 测试模式参考 IGES 的测试：用 OCCT 在内存中构造 box/sphere，写出 STEP 再读回，
 * 验证形状非空、类型一致；同时覆盖中文路径（develop 分支新增的特性）。
 */
#include "MeshData.h"
#include "ModelData.h"
#include "SplineData.h"
#include "StepModelHandler.h"
#include "TempFile.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <TopoDS_Shape.hxx>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

namespace fs = std::filesystem;

TEST_CASE("StepModelHandler::write_model()/read_model() - English path (box)")
{
    systems::io::StepModelHandler io;
    fs::path out = core::TempFile::instance().path().string() + ".stp";

    TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0);
    REQUIRE(!box.IsNull());

    auto spline_data = std::make_unique<SplineData>();
    spline_data->rootShape = std::make_unique<TopoDS_Shape>(box);

    std::unique_ptr<ModelData> model_write = std::make_unique<ModelData>(std::move(spline_data));
    REQUIRE_NOTHROW(io.write_model(*model_write, out, {}));
    REQUIRE(fs::exists(out));

    std::unique_ptr<ModelData> model_read;
    REQUIRE_NOTHROW(model_read = io.read_model(out, {}));
    REQUIRE(model_read != nullptr);

    const SplineData* read_spline = model_read->asSplineData();
    REQUIRE(read_spline != nullptr);
    REQUIRE(read_spline->rootShape != nullptr);
    REQUIRE(!read_spline->rootShape->IsNull());

    // model_name_ 应被设置为文件名
    REQUIRE(model_read->model_name_ == out.filename().string());
}

TEST_CASE("StepModelHandler::write_model()/read_model() - Chinese filename")
{
    // 验证 3e4f2ef 提交带来的中文路径支持（path.u8string()）
    systems::io::StepModelHandler io;

    fs::path out = core::TempFile::instance().path();
    out.replace_filename("中文_测试_" + out.stem().string() + ".stp");

    TopoDS_Shape box = BRepPrimAPI_MakeBox(15.0, 25.0, 35.0);
    REQUIRE(!box.IsNull());

    auto spline_data = std::make_unique<SplineData>();
    spline_data->rootShape = std::make_unique<TopoDS_Shape>(box);

    std::unique_ptr<ModelData> model_write = std::make_unique<ModelData>(std::move(spline_data));
    REQUIRE_NOTHROW(io.write_model(*model_write, out, {}));
    REQUIRE(fs::exists(out));

    std::unique_ptr<ModelData> model_read;
    REQUIRE_NOTHROW(model_read = io.read_model(out, {}));
    REQUIRE(model_read != nullptr);

    const SplineData* read_spline = model_read->asSplineData();
    REQUIRE(read_spline != nullptr);
    REQUIRE(read_spline->rootShape != nullptr);
    REQUIRE(!read_spline->rootShape->IsNull());
}

TEST_CASE("StepModelHandler::write_model()/read_model() - Chinese full path")
{
    systems::io::StepModelHandler io;

    fs::path out = core::TempFile::instance().path();
    out = out.parent_path() / "测试数据目录" / "子目录_模型"
        / ("中文_" + out.stem().string() + ".stp");
    std::filesystem::create_directories(out.parent_path());

    TopoDS_Shape box = BRepPrimAPI_MakeBox(12.0, 22.0, 32.0);
    REQUIRE(!box.IsNull());

    auto spline_data = std::make_unique<SplineData>();
    spline_data->rootShape = std::make_unique<TopoDS_Shape>(box);

    std::unique_ptr<ModelData> model_write = std::make_unique<ModelData>(std::move(spline_data));
    REQUIRE_NOTHROW(io.write_model(*model_write, out, {}));
    REQUIRE(fs::exists(out));

    std::unique_ptr<ModelData> model_read;
    REQUIRE_NOTHROW(model_read = io.read_model(out, {}));
    REQUIRE(model_read != nullptr);

    const SplineData* read_spline = model_read->asSplineData();
    REQUIRE(read_spline != nullptr);
    REQUIRE(read_spline->rootShape != nullptr);
    REQUIRE(!read_spline->rootShape->IsNull());
}

TEST_CASE("StepModelHandler::write_model()/read_model() - sphere")
{
    systems::io::StepModelHandler io;
    fs::path out = core::TempFile::instance().path().string() + "_sphere.stp";

    TopoDS_Shape sphere = BRepPrimAPI_MakeSphere(50.0);
    REQUIRE(!sphere.IsNull());

    auto spline_data = std::make_unique<SplineData>();
    spline_data->rootShape = std::make_unique<TopoDS_Shape>(sphere);

    std::unique_ptr<ModelData> model_write = std::make_unique<ModelData>(std::move(spline_data));
    REQUIRE_NOTHROW(io.write_model(*model_write, out, {}));
    REQUIRE(fs::exists(out));

    std::unique_ptr<ModelData> model_read;
    REQUIRE_NOTHROW(model_read = io.read_model(out, {}));
    REQUIRE(model_read != nullptr);

    const SplineData* read_spline = model_read->asSplineData();
    REQUIRE(read_spline != nullptr);
    REQUIRE(read_spline->rootShape != nullptr);
    REQUIRE(!read_spline->rootShape->IsNull());
}

TEST_CASE("StepModelHandler::read_model() - non-existent file")
{
    systems::io::StepModelHandler io;
    fs::path bad = core::TempFile::instance().path().string() + "_not_exist.stp";
    // 文件不存在，read_model 应返回 nullptr（而不是崩溃）
    std::unique_ptr<ModelData> model;
    REQUIRE_NOTHROW(model = io.read_model(bad, {}));
    REQUIRE(model == nullptr);
}

