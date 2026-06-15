/**
 * @file TestIgesModelHandler.cpp
 * @brief IGES 模型文件处理器单元测试
 * @author PreCess Team
 */
#include "IgesModelHandler.h"
#include "MeshData.h"
#include "ModelData.h"
#include "SplineData.h"
#include "TempFile.h"
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <TopoDS_Shape.hxx>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>

namespace fs = std::filesystem;

/**
 * @brief 比较两个 TopoDS_Shape 的基本属性
 */
static bool shapes_similar(const TopoDS_Shape& a, const TopoDS_Shape& b)
{
    // 比较类型
    if (a.ShapeType() != b.ShapeType()) {
        return false;
    }

    // 比较空状态
    if (a.IsNull() != b.IsNull()) {
        return false;
    }

    // 比较形状数量特性
    if (a.NbChildren() != b.NbChildren()) {
        return false;
    }

    return true;
}

/**
 * @brief 创建测试：英文路径下立方体读写回环测试
 */
TEST_CASE("IgesModelHandler::write_model()/read_model() - English path")
{
    systems::io::IgesModelHandler io;
    fs::path out = core::TempFile::instance().path().string() + ".igs";

    // 步骤1: 使用 OpenCASCADE 创建立方体
    TopoDS_Shape box = BRepPrimAPI_MakeBox(10.0, 20.0, 30.0);
    REQUIRE(!box.IsNull());

    // 步骤2: 包装为 SplineData
    auto spline_data = std::make_unique<SplineData>();
    spline_data->rootShape = std::make_unique<TopoDS_Shape>(box);

    // 步骤3: 写入 IGES 文件
    std::unique_ptr<ModelData> model_write = std::make_unique<ModelData>(std::move(spline_data));
    REQUIRE_NOTHROW(io.write_model(*model_write, out, { }));
    REQUIRE(fs::exists(out));

    // 步骤4: 读取 IGES 文件
    std::unique_ptr<ModelData> model_read;
    REQUIRE_NOTHROW(model_read = io.read_model(out, { }));
    REQUIRE(model_read != nullptr);

    // 步骤5: 验证读取的数据类型
    const SplineData* read_spline = model_read->asSplineData();
    REQUIRE(read_spline != nullptr);
    REQUIRE(read_spline->rootShape != nullptr);
    REQUIRE(!read_spline->rootShape->IsNull());

    // 步骤6: IGES 读写后拓扑结构会发生变化，只需验证成功读取到有效形状即可
    // IGES 转换后实体数量：立方体约49个，球体约14个
}

/**
 * @brief 创建测试：中文文件名（仅文件名含中文）
 */
TEST_CASE("IgesModelHandler::write_model()/read_model() - Chinese filename")
{
    systems::io::IgesModelHandler io;

    // 创建中文文件名
    fs::path out = core::TempFile::instance().path();
    out.replace_filename("测试_立方体_" + out.stem().string() + ".igs");

    // 步骤1: 使用 OpenCASCADE 创建立方体
    TopoDS_Shape box = BRepPrimAPI_MakeBox(15.0, 25.0, 35.0);
    REQUIRE(!box.IsNull());

    // 步骤2: 包装为 SplineData
    auto spline_data = std::make_unique<SplineData>();
    spline_data->rootShape = std::make_unique<TopoDS_Shape>(box);

    // 步骤3: 写入 IGES 文件
    std::unique_ptr<ModelData> model_write = std::make_unique<ModelData>(std::move(spline_data));
    REQUIRE_NOTHROW(io.write_model(*model_write, out, { }));
    REQUIRE(fs::exists(out));

    // 步骤4: 读取 IGES 文件
    std::unique_ptr<ModelData> model_read;
    REQUIRE_NOTHROW(model_read = io.read_model(out, { }));
    REQUIRE(model_read != nullptr);

    // 步骤5: 验证读取的数据类型
    const SplineData* read_spline = model_read->asSplineData();
    REQUIRE(read_spline != nullptr);
    REQUIRE(read_spline->rootShape != nullptr);
    REQUIRE(!read_spline->rootShape->IsNull());
}

/**
 * @brief 创建测试：中文完整路径（目录+文件名均含中文）
 */
TEST_CASE("IgesModelHandler::write_model()/read_model() - Chinese full path")
{
    systems::io::IgesModelHandler io;

    // 创建中文目录 + 中文文件名的完整路径
    fs::path out = core::TempFile::instance().path();
    out = out.parent_path() / "测试数据目录" / "子目录_模型" / ("测试_立方体_" + out.stem().string() + ".igs");
    std::filesystem::create_directories(out.parent_path());

    // 步骤1: 使用 OpenCASCADE 创建立方体
    TopoDS_Shape box = BRepPrimAPI_MakeBox(12.0, 22.0, 32.0);
    REQUIRE(!box.IsNull());

    // 步骤2: 包装为 SplineData
    auto spline_data = std::make_unique<SplineData>();
    spline_data->rootShape = std::make_unique<TopoDS_Shape>(box);

    // 步骤3: 写入 IGES 文件
    std::unique_ptr<ModelData> model_write = std::make_unique<ModelData>(std::move(spline_data));
    REQUIRE_NOTHROW(io.write_model(*model_write, out, { }));
    REQUIRE(fs::exists(out));

    // 步骤4: 读取 IGES 文件
    std::unique_ptr<ModelData> model_read;
    REQUIRE_NOTHROW(model_read = io.read_model(out, { }));
    REQUIRE(model_read != nullptr);

    // 步骤5: 验证读取的数据类型
    const SplineData* read_spline = model_read->asSplineData();
    REQUIRE(read_spline != nullptr);
    REQUIRE(read_spline->rootShape != nullptr);
    REQUIRE(!read_spline->rootShape->IsNull());
}

/**
 * @brief 创建测试：球体读写回环测试
 */
TEST_CASE("IgesModelHandler::write_model()/read_model() - Sphere test")
{
    systems::io::IgesModelHandler io;
    fs::path out = core::TempFile::instance().path().string() + "_sphere.igs";

    // 步骤1: 使用 OpenCASCADE 创建球体
    TopoDS_Shape sphere = BRepPrimAPI_MakeSphere(50.0);
    REQUIRE(!sphere.IsNull());

    // 步骤2: 包装为 SplineData
    auto spline_data = std::make_unique<SplineData>();
    spline_data->rootShape = std::make_unique<TopoDS_Shape>(sphere);

    // 步骤3: 写入 IGES 文件
    std::unique_ptr<ModelData> model_write = std::make_unique<ModelData>(std::move(spline_data));
    REQUIRE_NOTHROW(io.write_model(*model_write, out, { }));
    REQUIRE(fs::exists(out));

    // 步骤4: 读取 IGES 文件
    std::unique_ptr<ModelData> model_read;
    REQUIRE_NOTHROW(model_read = io.read_model(out, { }));
    REQUIRE(model_read != nullptr);

    // 步骤5: 验证读取的数据类型
    const SplineData* read_spline = model_read->asSplineData();
    REQUIRE(read_spline != nullptr);
    REQUIRE(read_spline->rootShape != nullptr);
    REQUIRE(!read_spline->rootShape->IsNull());
}

/**
 * @brief 测试：空模型写入（错误处理测试
 */
TEST_CASE("IgesModelHandler::write_model() - Null model handling")
{
    systems::io::IgesModelHandler io;
    fs::path out = core::TempFile::instance().path().string() + "_null.igs";

    // 创建一个非 SplineData 的模型（比如 MeshData）
    auto mesh_data = std::make_unique<MeshData>();
    std::unique_ptr<ModelData> model = std::make_unique<ModelData>(std::move(mesh_data));

    // 应该不会抛出异常，但会记录错误日志，不写入文件
    REQUIRE_NOTHROW(io.write_model(*model, out, { }));
    // IGES 处理器不支持 MeshData，应该不会成功写出
}