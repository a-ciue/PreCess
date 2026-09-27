/**
 * @file TestQhexModelHandler.cpp
 * @brief QhexModelHandler 的单元测试：格式解析、错误处理与多组件合并导出
 */
#include "ComponentData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelPayload.h"
#include "QhexModelHandler.h"
#include "TempFile.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {
//! @brief 单位立方体六面体网格：8 点 1 个 VTK 序六面体（右手法则、外向）
MeshData makeHexMesh()
{
    MeshData mesh;
    mesh.init();
    mesh.vertex_positions_ = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 1.0, 1.0, 0.0 },
        { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 },
        { 1.0, 0.0, 1.0 },
        { 1.0, 1.0, 1.0 },
        { 0.0, 1.0, 1.0 },
    };
    mesh.solid_types_ = { 12 }; // VTK_HEXAHEDRON
    mesh.solid_vertices_ = { 0, 1, 2, 3, 4, 5, 6, 7 };
    mesh.solid_vertices_offset_ = { 0, 8 };
    mesh.vertex_count_ = 8;
    return mesh;
}

//! @brief 写出一份测试用 Qhex 文本文件
void writeFile(const fs::path& path, const std::string& content)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.is_open());
    output << content;
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

/**
 * @brief 读回文件并返回 payload 本体
 *
 * 按值返回：组件由 unique_ptr 持有，返回裸指针会随局部 payload 析构而悬空。
 */
ModelPayload requireReadPayload(systems::io::QhexModelHandler& handler, const fs::path& path)
{
    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = handler.read_model(path, {}));
    REQUIRE(payload.has_value());
    REQUIRE(!payload->components.empty());
    REQUIRE(payload->components.front()->mesh);
    return std::move(*payload);
}
} // namespace

TEST_CASE("QhexModelHandler reads vertices and hexahedra with 1-based ids")
{
    systems::io::QhexModelHandler handler;
    const fs::path input = core::TempFile::instance().path().string() + "_read.qhex";

    // 顶点 id 与空间顺序错开且不连续编号，检验 id 映射而非顺序假设
    writeFile(input,
        "Vertex 1 0 0 0\r\n"
        "Vertex 3 1 0 0\r\n"
        "Vertex 2 1 1 0\r\n"
        "Vertex 5 0 1 0\r\n"
        "Vertex 4 0 0 1\r\n"
        "Vertex 7 1 0 1\r\n"
        "Vertex 6 1 1 1\r\n"
        "Vertex 8 0 1 1\r\n"
        "Hex 1 1 3 2 5 4 7 6 8\r\n");

    const ModelPayload payload = requireReadPayload(handler, input);
    const MeshData* mesh = payload.components.front()->mesh.get();
    REQUIRE(mesh->vertex_positions_.size() == 8);
    REQUIRE(mesh->vertex_count_ == 8);
    // 六面体引用的文件点 id 1,3,2,5,4,7,6,8 按出现顺序换算为局部点 0..7
    REQUIRE(mesh->solid_types_ == std::vector<unsigned char> { 12 });
    REQUIRE(mesh->solid_vertices_ == std::vector<Index> { 0, 1, 2, 3, 4, 5, 6, 7 });
    REQUIRE(mesh->solid_vertices_offset_ == std::vector<Index> { 0, 8 });
    // 模型名取文件名（TempFile 路径随机，校验后缀）
    const std::string suffix = "_read.qhex";
    REQUIRE(payload.model_name.size() > suffix.size());
    REQUIRE(payload.model_name.compare(payload.model_name.size() - suffix.size(), suffix.size(), suffix) == 0);
}

TEST_CASE("QhexModelHandler skips unknown keyword lines")
{
    systems::io::QhexModelHandler handler;
    const fs::path input = core::TempFile::instance().path().string() + "_unknown.qhex";

    // 未知关键字行（格式的版本演进）告警后跳过，不阻塞已知记录
    writeFile(input,
        "Comment something arbitrary\n"
        "Vertex 1 0 0 0\n"
        "Vertex 2 1 0 0\n"
        "Vertex 3 1 1 0\n"
        "Vertex 4 0 1 0\n"
        "Vertex 5 0 0 1\n"
        "Vertex 6 1 0 1\n"
        "Vertex 7 1 1 1\n"
        "Vertex 8 0 1 1\n"
        "Face 1 1 2 3 4\n"
        "Hex 1 1 2 3 4 5 6 7 8\n");

    const ModelPayload payload = requireReadPayload(handler, input);
    const MeshData* mesh = payload.components.front()->mesh.get();
    REQUIRE(mesh->vertex_positions_.size() == 8);
    REQUIRE(mesh->solid_vertices_offset_ == std::vector<Index> { 0, 8 });
}

TEST_CASE("QhexModelHandler rejects corrupt records")
{
    systems::io::QhexModelHandler handler;

    const std::string cube_vertices =
        "Vertex 1 0 0 0\n"
        "Vertex 2 1 0 0\n"
        "Vertex 3 1 1 0\n"
        "Vertex 4 0 1 0\n"
        "Vertex 5 0 0 1\n"
        "Vertex 6 1 0 1\n"
        "Vertex 7 1 1 1\n"
        "Vertex 8 0 1 1\n";

    SECTION("hex references unknown vertex id")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_badref.qhex";
        writeFile(input, cube_vertices + "Hex 1 1 2 3 4 5 6 7 99\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("duplicate vertex id")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_dupvid.qhex";
        writeFile(input, cube_vertices + "Vertex 1 9 9 9\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("hex record truncated below 8 corners")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_short.qhex";
        writeFile(input, cube_vertices + "Hex 1 1 2 3 4 5 6 7\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("vertex record missing coordinates")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_badvert.qhex";
        writeFile(input, "Vertex 1 0 0\n" + cube_vertices.substr(cube_vertices.find("Vertex 2")) + "Hex 1 1 2 3 4 5 6 7 8\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("no hex records")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_nohex.qhex";
        writeFile(input, cube_vertices);
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("empty file")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_empty.qhex";
        writeFile(input, "\r\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("missing file")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_missing.qhex";
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }
}

TEST_CASE("QhexModelHandler write_components round-trips a hexahedral mesh")
{
    systems::io::QhexModelHandler handler;
    const fs::path out = core::TempFile::instance().path().string() + "_roundtrip.qhex";

    ModelLayer layer;
    const std::vector<Index> component_ids = addMeshComponent(layer, std::make_unique<MeshData>(makeHexMesh()));

    REQUIRE_NOTHROW(handler.write_components(layer, component_ids, out, {}));
    REQUIRE(fs::exists(out));

    const ModelPayload payload = requireReadPayload(handler, out);
    const MeshData* mesh = payload.components.front()->mesh.get();
    REQUIRE(mesh->vertex_positions_.size() == 8);
    REQUIRE(mesh->solid_types_ == std::vector<unsigned char> { 12 });
    REQUIRE(mesh->solid_vertices_ == std::vector<Index> { 0, 1, 2, 3, 4, 5, 6, 7 });
    REQUIRE(mesh->solid_vertices_offset_ == std::vector<Index> { 0, 8 });
}

TEST_CASE("QhexModelHandler write_components merges components with point offset")
{
    systems::io::QhexModelHandler handler;
    const fs::path out = core::TempFile::instance().path().string() + "_merge.qhex";

    // 两个相同立方体组件：合并后 16 点 2 个六面体，第二个组件的点引用整体偏移 8
    ModelLayer layer;
    const std::vector<Index> first_ids = addMeshComponent(layer, std::make_unique<MeshData>(makeHexMesh()));
    const std::vector<Index> second_ids = addMeshComponent(layer, std::make_unique<MeshData>(makeHexMesh()));

    std::vector<Index> component_ids = first_ids;
    component_ids.insert(component_ids.end(), second_ids.begin(), second_ids.end());

    REQUIRE_NOTHROW(handler.write_components(layer, component_ids, out, {}));
    REQUIRE(fs::exists(out));

    const ModelPayload payload = requireReadPayload(handler, out);
    const MeshData* mesh = payload.components.front()->mesh.get();
    REQUIRE(mesh->vertex_positions_.size() == 16);
    REQUIRE(mesh->solid_types_.size() == 2);
    REQUIRE(mesh->solid_vertices_offset_ == std::vector<Index> { 0, 8, 16 });
    // 第二个组件的六面体引用偏移后的点（8..15）
    REQUIRE(mesh->solid_vertices_[8] >= 8);
    REQUIRE(mesh->solid_vertices_.back() < 16);
}

TEST_CASE("QhexModelHandler write_components skips non-hex solids and refuses hex-free export")
{
    systems::io::QhexModelHandler handler;
    const fs::path out = core::TempFile::instance().path().string() + "_nonhex.qhex";

    // 立方体 + 一个三棱柱（VTK_WEDGE=13）：导出只保留六面体
    auto mixed = std::make_unique<MeshData>(makeHexMesh());
    mixed->solid_types_.push_back(13);
    mixed->solid_vertices_.insert(mixed->solid_vertices_.end(), { 0, 1, 2, 3, 4, 5 });
    mixed->solid_vertices_offset_.push_back(14);

    ModelLayer layer;
    const std::vector<Index> component_ids = addMeshComponent(layer, std::move(mixed));

    REQUIRE_NOTHROW(handler.write_components(layer, component_ids, out, {}));
    REQUIRE(fs::exists(out));

    const ModelPayload payload = requireReadPayload(handler, out);
    const MeshData* mesh = payload.components.front()->mesh.get();
    REQUIRE(mesh->solid_types_ == std::vector<unsigned char> { 12 });
    REQUIRE(mesh->solid_vertices_offset_ == std::vector<Index> { 0, 8 });

    // 全部为非六面体组件时无可导出内容，不产生文件
    const fs::path out_empty = core::TempFile::instance().path().string() + "_hexfree.qhex";
    auto wedge_only = std::make_unique<MeshData>();
    wedge_only->init();
    wedge_only->vertex_positions_ = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 },
        { 1.0, 0.0, 1.0 },
        { 0.0, 1.0, 1.0 },
    };
    wedge_only->solid_types_ = { 13 };
    wedge_only->solid_vertices_ = { 0, 1, 2, 3, 4, 5 };
    wedge_only->solid_vertices_offset_ = { 0, 6 };

    ModelLayer wedge_layer;
    const std::vector<Index> wedge_ids = addMeshComponent(wedge_layer, std::move(wedge_only));
    REQUIRE_NOTHROW(handler.write_components(wedge_layer, wedge_ids, out_empty, {}));
    REQUIRE_FALSE(fs::exists(out_empty));
}
