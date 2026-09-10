/**
 * @file TestModelIOFileName.cpp
 * @brief 导出文件名/扩展名推导（ModelIOSystem::suggestFileName、adaptFileExtension）的单元测试
 */
#include "ArgType.h"
#include "ModelIOHandler.h"
#include "ModelIOSystem.h"
#include "ModelLayer.h"
#include "ModelPayload.h"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string>

using namespace systems::io;
namespace fs = std::filesystem;

namespace {
//! @brief 只提供文件类型信息的最小 handler：本用例只验证文件名推导，不涉及真实读写
class FakeIOHandler : public ModelIOHandler {
public:
    std::optional<ModelPayload> read_model(const fs::path&, const std::vector<std::any>&) override
    {
        return std::nullopt;
    }
    void write_components(const ModelLayer&, const std::vector<Index>&, const fs::path&, const std::vector<std::any>&) override
    {
    }
    std::vector<core::ArgType> read_args_type() const override
    {
        return {};
    }
    std::vector<core::ArgType> write_args_type() const override
    {
        return {};
    }
};

//! @brief 注册若干文件类型，构造出被测的 IO 系统
class IOFixture {
public:
    ModelLayer layer;
    ModelIOSystem system { layer };

    IOFixture()
    {
        registerType("obj", { "obj" });
        registerType("stl", { "stl" });
        registerType("iges", { "iges", "igs" });
    }

private:
    void registerType(const std::string& file_type, const std::vector<std::string>& extensions)
    {
        ModelIOSystem::SystemHandlerPtr handler { new FakeIOHandler() };
        REQUIRE(system.registerHandler(HandlerMetaData { file_type, extensions }, std::move(handler)));
    }
};
}

TEST_CASE("ModelIOSystem::suggestFileName adapts model name to file type", "[ModelIOSystem]")
{
    IOFixture f;

    // 同名类型换扩展名保持不动，异名类型换成目标类型的首选扩展名
    REQUIRE(f.system.suggestFileName("cube.obj", "obj") == "cube.obj");
    REQUIRE(f.system.suggestFileName("cube.obj", "stl") == "cube.stl");
    // 扩展名比较大小写不敏感
    REQUIRE(f.system.suggestFileName("cube.OBJ", "stl") == "cube.stl");
    // 目标类型有多个扩展名时取第一个
    REQUIRE(f.system.suggestFileName("cube.igs", "iges") == "cube.iges");
    // 模型名不带扩展名时直接补上
    REQUIRE(f.system.suggestFileName("cube", "stl") == "cube.stl");
    REQUIRE(f.system.suggestFileName("", "stl").empty());
}

TEST_CASE("ModelIOSystem::suggestFileName keeps model name for unknown file type", "[ModelIOSystem]")
{
    IOFixture f;

    // "All files" 是文件对话框的未注册默认项：保留模型名原样，其扩展名是反查文件类型的唯一线索
    REQUIRE(f.system.suggestFileName("cube.obj", "All files") == "cube.obj");
    REQUIRE(f.system.suggestFileName("cube.obj", "unknown_type") == "cube.obj");
    // 未注册的扩展名视为名字的一部分，不剥除
    REQUIRE(f.system.suggestFileName("part.v2", "stl") == "part.v2.stl");
}

TEST_CASE("ModelIOSystem::adaptFileExtension completes and replaces extension", "[ModelIOSystem]")
{
    IOFixture f;
    const fs::path dir = fs::path("out") / "models";

    // 缺失扩展名时补齐，属于其他已注册类型时替换
    REQUIRE(f.system.adaptFileExtension(dir / "cube", "stl") == dir / "cube.stl");
    REQUIRE(f.system.adaptFileExtension(dir / "cube.obj", "stl") == dir / "cube.stl");
    // 已属于目标类型时保持不动（含大小写差异）
    REQUIRE(f.system.adaptFileExtension(dir / "cube.stl", "stl") == dir / "cube.stl");
    REQUIRE(f.system.adaptFileExtension(dir / "cube.OBJ", "obj") == dir / "cube.OBJ");
    // 用户显式指定的未注册扩展名不被改写
    REQUIRE(f.system.adaptFileExtension(dir / "cube.dat", "stl") == dir / "cube.dat");
    // 目标类型未注册时无从校正
    REQUIRE(f.system.adaptFileExtension(dir / "cube", "All files") == dir / "cube");
}
