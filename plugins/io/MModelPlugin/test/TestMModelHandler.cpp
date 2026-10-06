/**
 * @file TestMModelHandler.cpp
 * @brief M (.m) 网格文件处理器单元测试
 *
 * MModelHandler 直接解析 / 生成 .m 文本读写网格，当前写出入口为组件化
 * write_components()。MeshData 自包含（vertex_positions_ 常驻坐标、连通性存组件内局部点索引），
 * 测试将源 MeshData 加入 ModelLayer 后即可直接导出，无需全局点池换算。
 * .m 格式以表面三角网格为主，不支持体单元，写出后体信息不会回流。
 * 点/面 {...} 属性段经 v_<key>_<分量数> / f_<key>_<分量数> 命名的属性表回环。
 */
#include "MModelHandler.h"
#include "ComponentData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelPayload.h"
#include "TempFile.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace fs = std::filesystem;

namespace {
/**
 * @brief 构造一个仅含表面三角形的最小 MeshData
 *
 * 源数据仍带一个 patch/block，用于验证写出不依赖（也不再保留）分组信息。
 */
MeshData MakeSurfaceTriMesh()
{
    MeshData m;
    m.init();
    m.vertex_positions_ = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 },
    };
    m.face_vertices_ = {
        0, 2, 1,
        0, 1, 3,
        0, 3, 2,
        1, 2, 3,
    };
    m.face_vertices_offset_ = { 0, 3, 6, 9, 12 };

    auto patch = std::make_unique<Patch>(1, 1);
    patch->faces = { 0, 1, 2, 3 };
    m.patches_[1] = std::move(patch);

    auto block = std::make_unique<Block>();
    block->id = 1;
    block->patchIDs = { 1 };
    m.blocks_[1] = std::move(block);
    return m;
}

std::vector<Index> addMeshModelAndGetComponentIds(
    ModelLayer& layer,
    std::unique_ptr<MeshData> mesh)
{
    ComponentDatas comps;
    auto comp = std::make_unique<ComponentData>();
    comp->id = -1;
    comp->mesh = std::move(mesh);
    comps.push_back(std::move(comp));
    const Index modelId = layer.addModel("model", std::move(comps));
    REQUIRE(modelId >= 0);

    std::vector<Index> componentIds = layer.modelById(modelId)->componentIds();
    REQUIRE(!componentIds.empty());

    return componentIds;
}

const MeshData* requireReadableMeshModel(const ModelPayload& payload)
{
    const auto& components = payload.components;
    REQUIRE(!components.empty());

    for (const auto& component : components) {
        if (component && component->mesh) {
            return component->mesh.get();
        }
    }

    FAIL("read model does not contain MeshData component");
    return nullptr;
}
} // namespace

TEST_CASE("MModelHandler::write_components()/read_model() round-trip")
{
    systems::io::MModelHandler io;

    fs::path out;
    SECTION("Latin path")
    {
        out = core::TempFile::instance().path().string() + ".m";
    }
    SECTION("Chinese filename")
    {
        out = core::TempFile::instance().path();
        out.replace_filename("中文_" + out.stem().string() + ".m");
    }

    // MeshData 含 unique_ptr，不可拷贝；分别构造源与参照
    ModelLayer layer;
    std::vector<Index> componentIds = addMeshModelAndGetComponentIds(
        layer,
        std::make_unique<MeshData>(MakeSurfaceTriMesh()));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());

    const MeshData* read_mesh = requireReadableMeshModel(*payload);

    const MeshData ref = MakeSurfaceTriMesh();
    // 顶点数应一致
    REQUIRE(read_mesh->vertex_positions_.size() == ref.vertex_positions_.size());
    // 面数应一致
    REQUIRE(read_mesh->face_vertices_offset_.size() == ref.face_vertices_offset_.size());
    REQUIRE(read_mesh->face_vertices_.size() == ref.face_vertices_.size());

    // 顶点坐标精确对比（.m 是文本格式，应可精确恢复）
    for (size_t i = 0; i < ref.vertex_positions_.size(); ++i) {
        for (int k = 0; k < 3; ++k) {
            REQUIRE(read_mesh->vertex_positions_[i][k] == ref.vertex_positions_[i][k]);
        }
    }

    // patches_/blocks_ 已废弃，.m 读写不再维护 patch 分组，
    // 读回网格不重建 patches_，面数据完整回流即可。
    REQUIRE(read_mesh->patches_.empty());
}

TEST_CASE("MModelHandler::attribute round-trip")
{
    systems::io::MModelHandler io;
    fs::path out = core::TempFile::instance().path().string() + "_attr.m";

    // 点/面属性按 v_<key>_<分量数> / f_<key>_<分量数> 命名，
    // 经 .m {...} 属性段写出并读回后应原样保留
    MeshData m = MakeSurfaceTriMesh();
    m.vertex_attributes_["v_rgb_3"] = {
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0,
        0.5, 0.5, 0.5,
    };
    m.face_attributes_["f_g_1"] = { 1.0, 1.0, 2.0, 2.0 };
    const std::vector<double> ref_rgb = m.vertex_attributes_["v_rgb_3"];
    const std::vector<double> ref_g = m.face_attributes_["f_g_1"];

    ModelLayer layer;
    std::vector<Index> componentIds = addMeshModelAndGetComponentIds(
        layer,
        std::make_unique<MeshData>(std::move(m)));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());

    const MeshData* read_mesh = requireReadableMeshModel(*payload);
    REQUIRE(read_mesh->vertex_attributes_.count("v_rgb_3") == 1);
    REQUIRE(read_mesh->vertex_attributes_.at("v_rgb_3") == ref_rgb);
    REQUIRE(read_mesh->face_attributes_.count("f_g_1") == 1);
    REQUIRE(read_mesh->face_attributes_.at("f_g_1") == ref_g);
}

TEST_CASE("MModelHandler::write_components() without patches preserves faces")
{
    systems::io::MModelHandler io;
    fs::path out = core::TempFile::instance().path().string() + "_empty.m";

    MeshData m;
    m.init();
    m.vertex_positions_ = { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 } };
    m.face_vertices_ = { 0, 1, 2 };
    m.face_vertices_offset_ = { 0, 3 };
    // 注意：没有 patches_（patches_/blocks_ 已废弃，写出不再依赖分组信息）

    ModelLayer layer;
    std::vector<Index> componentIds = addMeshModelAndGetComponentIds(
        layer,
        std::make_unique<MeshData>(std::move(m)));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    // 无 patch 时面数据仍应完整写出并读回
    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());

    const MeshData* read_mesh = requireReadableMeshModel(*payload);
    // 读回应保留写出的 1 个三角形面
    REQUIRE(read_mesh->face_vertices_offset_.size() == 2);
}

TEST_CASE("MModelHandler::read_model() - model_name preserved")
{
    systems::io::MModelHandler io;
    fs::path out = core::TempFile::instance().path().string() + "_name.m";

    ModelLayer layer;
    std::vector<Index> componentIds = addMeshModelAndGetComponentIds(
        layer,
        std::make_unique<MeshData>(MakeSurfaceTriMesh()));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());
    REQUIRE(payload->model_name == out.filename().u8string());
}

TEST_CASE("MModelHandler::read_model() preserves Edge attributes", "[MModelHandler][Edge]")
{
    systems::io::MModelHandler io;
    const fs::path path = core::TempFile::instance().path().string() + "_edges.m";
    const std::string vertices = "Vertex 1946 0 0 0\n"
                                 "Vertex 748 1 0 0\n"
                                 "Vertex 42 0 1 0\n";
    const std::string edges = "Edge 42 1946\n"
                              "Edge 1946 748 {loop_id=(9) candidate_id=(0) segment_id=(0) loop_type=(3) source_path_id=(0) is_sharp=(0) accepted=(1) reject_reason=(0) homology_ok=(1) energy=(1.9067301061999999) length_term=(1.876211718) planarity_term=(0.030518388216) concave_score=(0)}\n"
                              "Edge 748 42 {loop_id=(2) direction=(1 0 -1)}\n"
                              "Edge 748 1946\n";

    // Edge 只有两个端点编号；延后映射应同时兼容边记录出现在顶点之前和之后。
    std::ofstream output(path);
    REQUIRE(output.is_open());
    SECTION("Edges after vertices")
    {
        output << vertices << "Face 7 1946 748 42 {g=(5)}\n"
               << edges;
    }
    SECTION("Edges before vertices")
    {
        output << edges << vertices << "Face 7 1946 748 42 {g=(5)}\n";
    }
    output.close();

    const auto payload = io.read_model(path, { });
    REQUIRE(payload.has_value());
    const MeshData* mesh = requireReadableMeshModel(*payload);
    const std::vector<Index> expected_edges = { 2, 0, 0, 1, 1, 2, 1, 0 };
    REQUIRE(mesh->edge_vertices_ == expected_edges);
    REQUIRE(mesh->vertex_count_ == 3);
    const std::vector<Index> expected_faces = { 0, 1, 2 };
    REQUIRE(mesh->face_vertices_ == expected_faces);
    REQUIRE(mesh->face_attributes_.at("f_g_1") == std::vector<double> { 5.0 });

    // 按边单元顺序验证完整示例的全部标量，缺失属性的前后边均须补零。
    const std::map<std::string, double> expected_scalars = {
        { "loop_id", 9.0 }, { "candidate_id", 0.0 }, { "segment_id", 0.0 },
        { "loop_type", 3.0 }, { "source_path_id", 0.0 }, { "is_sharp", 0.0 },
        { "accepted", 1.0 }, { "reject_reason", 0.0 }, { "homology_ok", 1.0 },
        { "energy", 1.9067301061999999 }, { "length_term", 1.876211718 },
        { "planarity_term", 0.030518388216 }, { "concave_score", 0.0 }
    };
    REQUIRE(mesh->edge_attributes_.size() == expected_scalars.size() + 1);
    for (const auto& [key, value] : expected_scalars) {
        const std::string name = "e_" + key + "_1";
        CAPTURE(name);
        const std::vector<double> expected = { 0.0, value, key == "loop_id" ? 2.0 : 0.0, 0.0 };
        REQUIRE(mesh->edge_attributes_.at(name) == expected);
    }
    const std::vector<double> expected_direction = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, -1.0, 0.0, 0.0, 0.0 };
    REQUIRE(mesh->edge_attributes_.at("e_direction_3") == expected_direction);
    REQUIRE(mesh->vertex_attributes_.empty());
}

TEST_CASE("MModelHandler::read_model() skips invalid Edge records", "[MModelHandler][Edge]")
{
    systems::io::MModelHandler io;
    const fs::path path = core::TempFile::instance().path().string() + "_invalid_edges.m";
    std::ofstream output(path);
    REQUIRE(output.is_open());
    // 非法端点、缺失端点和自环不能产生边单元或让后续合法边的属性错位。
    output << "Vertex 10 0 0 0\nVertex 20 1 0 0\n"
              "Edge 10 99 {invalid=(1)}\n"
              "Edge 10 {invalid=(2)}\n"
              "Edge 10 10 {invalid=(3)}\n"
              "Edge 10 20 {accepted=(1)}\n";
    output.close();

    const auto payload = io.read_model(path, { });
    REQUIRE(payload.has_value());
    const MeshData* mesh = requireReadableMeshModel(*payload);
    const std::vector<Index> expected_edges = { 0, 1 };
    REQUIRE(mesh->edge_vertices_ == expected_edges);
    REQUIRE(mesh->edge_attributes_.size() == 1);
    REQUIRE(mesh->edge_attributes_.at("e_accepted_1") == std::vector<double> { 1.0 });
}
