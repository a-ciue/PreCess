/**
 * @file TestStlModelHandler.cpp
 * @brief StlModelHandler 的单元测试：ASCII/二进制解析、顶点焊接、错误处理与多组件合并导出
 */
#include "ComponentData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelPayload.h"
#include "StlModelHandler.h"
#include "TempFile.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

//! @brief 两个三角形共享一条边的正方形：soup 6 顶点焊接后应只剩 4 个
const char* kSquareAsciiStl = "solid square\r\n"
                              "  facet normal 0 0 1\r\n"
                              "    outer loop\r\n"
                              "      vertex 0 0 0\r\n"
                              "      vertex 1 0 0\r\n"
                              "      vertex 1 1 0\r\n"
                              "    endloop\r\n"
                              "  endfacet\r\n"
                              "  facet normal 0 0 1\r\n"
                              "    outer loop\r\n"
                              "      vertex 0 0 0\r\n"
                              "      vertex 1 1 0\r\n"
                              "      vertex 0 1 0\r\n"
                              "    endloop\r\n"
                              "  endfacet\r\n"
                              "endsolid square\r\n";

//! @brief 写出一份测试用文本文件
void writeFile(const fs::path& path, const std::string& content)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.is_open());
    output << content;
}

//! @brief 构造单三角形二进制 STL（头 80 字节、计数 1、法向 +0 z + 三顶点 + 属性 0）
std::string makeBinaryStl(float x0, float y0)
{
    std::string bytes(80, '\0');
    const uint32_t facet_count = 1;
    bytes.append(reinterpret_cast<const char*>(&facet_count), sizeof(facet_count));
    const auto put = [&bytes](float v) {
        bytes.append(reinterpret_cast<const char*>(&v), sizeof(v));
    };
    put(0.0f);
    put(0.0f);
    put(1.0f); // normal
    put(x0);
    put(y0);
    put(0.0f);
    put(x0 + 1.0f);
    put(y0);
    put(0.0f);
    put(x0);
    put(y0 + 1.0f);
    put(0.0f);
    const uint16_t attribute = 0;
    bytes.append(reinterpret_cast<const char*>(&attribute), sizeof(attribute));
    REQUIRE(bytes.size() == 84 + 50);
    return bytes;
}

//! @brief 把网格作为单组件加入模型层，返回组件 id 列表
std::vector<Index> addMeshComponent(ModelLayer& layer, std::unique_ptr<MeshData> mesh)
{
    ComponentDatas components;
    auto component = std::make_unique<ComponentData>();
    component->id = -1;
    component->mesh = std::move(mesh);
    components.push_back(std::move(component));

    const Index model_id = layer.addModel("model", std::move(components));
    REQUIRE(model_id >= 0);
    const std::vector<Index> component_ids = layer.modelById(model_id)->componentIds();
    REQUIRE(!component_ids.empty());
    return component_ids;
}

//! @brief 单位正方形的两个三角形（共享 4 顶点），x 方向平移 offset 用于错开多组件
MeshData makeSquareMesh(double offset_x)
{
    MeshData mesh;
    mesh.init();
    mesh.vertex_positions_ = {
        { offset_x + 0.0, 0.0, 0.0 },
        { offset_x + 1.0, 0.0, 0.0 },
        { offset_x + 1.0, 1.0, 0.0 },
        { offset_x + 0.0, 1.0, 0.0 },
    };
    mesh.vertex_count_ = 4;
    mesh.face_vertices_ = { 0, 1, 2, 0, 2, 3 };
    mesh.face_vertices_offset_ = { 0, 3, 6 };
    return mesh;
}

/**
 * @brief 读回文件并返回 payload 本体
 *
 * 按值返回：组件由 unique_ptr 持有，返回裸指针会随局部 payload 析构而悬空。
 */
ModelPayload requireReadPayload(systems::io::StlModelHandler& handler, const fs::path& path)
{
    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = handler.read_model(path, {}));
    REQUIRE(payload.has_value());
    REQUIRE(!payload->components.empty());
    REQUIRE(payload->components.front()->mesh);
    return std::move(*payload);
}
} // namespace

TEST_CASE("StlModelHandler reads ASCII STL and welds shared vertices")
{
    systems::io::StlModelHandler handler;
    const fs::path input = core::TempFile::instance().path().string() + "_read.stl";

    writeFile(input, kSquareAsciiStl);
    const ModelPayload payload = requireReadPayload(handler, input);
    const MeshData* mesh = payload.components.front()->mesh.get();

    // soup 6 顶点按坐标精确焊接为 4 顶点，面连通性保持 2 个三角形
    REQUIRE(mesh->vertex_count_ == 4);
    REQUIRE(mesh->vertex_positions_.size() == 4);
    REQUIRE(mesh->face_vertices_ == std::vector<Index> { 0, 1, 2, 0, 2, 3 });
    REQUIRE(mesh->face_vertices_offset_ == std::vector<Index> { 0, 3, 6 });
}

TEST_CASE("StlModelHandler reads binary STL with welded vertices")
{
    systems::io::StlModelHandler handler;
    const fs::path input = core::TempFile::instance().path().string() + "_bin.stl";

    writeFile(input, makeBinaryStl(0.0f, 0.0f));
    const ModelPayload payload = requireReadPayload(handler, input);
    const MeshData* mesh = payload.components.front()->mesh.get();
    REQUIRE(mesh->vertex_count_ == 3);
    REQUIRE(mesh->vertex_positions_[1] == std::array<double, 3> { 1.0, 0.0, 0.0 });
    REQUIRE(mesh->face_vertices_offset_ == std::vector<Index> { 0, 3 });
}

TEST_CASE("StlModelHandler rejects corrupt files")
{
    systems::io::StlModelHandler handler;

    SECTION("facet missing a vertex")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_short.stl";
        writeFile(input,
            "solid x\r\n"
            "  facet normal 0 0 1\r\n"
            "    outer loop\r\n"
            "      vertex 0 0 0\r\n"
            "      vertex 1 0 0\r\n"
            "    endloop\r\n"
            "  endfacet\r\n"
            "endsolid x\r\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("bad vertex coordinates")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_badvert.stl";
        writeFile(input,
            "solid x\r\n"
            "  facet normal 0 0 1\r\n"
            "    outer loop\r\n"
            "      vertex 0 0 not-a-number\r\n"
            "      vertex 1 0 0\r\n"
            "      vertex 1 1 0\r\n"
            "    endloop\r\n"
            "  endfacet\r\n"
            "endsolid x\r\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("no facet record")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_nofacet.stl";
        writeFile(input, "solid x\r\nendsolid x\r\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("binary size mismatch")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_badsize.stl";
        const std::string good = makeBinaryStl(0.0f, 0.0f);
        writeFile(input, good.substr(0, good.size() - 10)); // 布局自洽性破坏且非 ASCII
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("missing file")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_missing.stl";
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }
}

TEST_CASE("StlModelHandler write_components round-trips a triangle mesh")
{
    systems::io::StlModelHandler handler;
    const fs::path out = core::TempFile::instance().path().string() + "_roundtrip.stl";

    ModelLayer layer;
    const std::vector<Index> component_ids = addMeshComponent(layer, std::make_unique<MeshData>(makeSquareMesh(0.0)));

    REQUIRE_NOTHROW(handler.write_components(layer, component_ids, out, {}));
    REQUIRE(fs::exists(out));

    // 写出为 ASCII，读回后焊接应恰好还原共享顶点的 4 点 2 三角形
    const ModelPayload payload = requireReadPayload(handler, out);
    const MeshData* mesh = payload.components.front()->mesh.get();
    REQUIRE(mesh->vertex_count_ == 4);
    REQUIRE(mesh->face_vertices_.size() == 6);
    REQUIRE(mesh->face_vertices_offset_ == std::vector<Index> { 0, 3, 6 });
}

TEST_CASE("StlModelHandler write_components merges components with point offset")
{
    systems::io::StlModelHandler handler;
    const fs::path out = core::TempFile::instance().path().string() + "_merge.stl";

    // 两个正方形组件：合并导出后 8 点 4 三角形
    ModelLayer layer;
    // 第二个组件 x 平移 5，避免与第一个组件坐标重叠（STL 读回按坐标焊接）
    const std::vector<Index> first_ids = addMeshComponent(layer, std::make_unique<MeshData>(makeSquareMesh(0.0)));
    const std::vector<Index> second_ids = addMeshComponent(layer, std::make_unique<MeshData>(makeSquareMesh(5.0)));

    std::vector<Index> component_ids = first_ids;
    component_ids.insert(component_ids.end(), second_ids.begin(), second_ids.end());

    REQUIRE_NOTHROW(handler.write_components(layer, component_ids, out, {}));
    REQUIRE(fs::exists(out));

    const ModelPayload payload = requireReadPayload(handler, out);
    const MeshData* mesh = payload.components.front()->mesh.get();
    REQUIRE(mesh->vertex_count_ == 8);
    REQUIRE(mesh->face_vertices_offset_ == std::vector<Index> { 0, 3, 6, 9, 12 });
}

TEST_CASE("StlModelHandler write_components skips non-triangle faces and refuses empty export")
{
    systems::io::StlModelHandler handler;

    // 一个四边形面（未三角化）：STL 无法承载，导出整体失败且不产生文件
    MeshData quad_only;
    quad_only.init();
    quad_only.vertex_positions_ = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 1.0, 1.0, 0.0 },
        { 0.0, 1.0, 0.0 },
    };
    quad_only.vertex_count_ = 4;
    quad_only.face_vertices_ = { 0, 1, 2, 3 };
    quad_only.face_vertices_offset_ = { 0, 4 };

    ModelLayer layer;
    const std::vector<Index> component_ids = addMeshComponent(layer, std::make_unique<MeshData>(std::move(quad_only)));

    const fs::path out = core::TempFile::instance().path().string() + "_quad.stl";
    REQUIRE_NOTHROW(handler.write_components(layer, component_ids, out, {}));
    REQUIRE_FALSE(fs::exists(out));
}
