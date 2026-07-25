#include "InpModelHandler.h"
#include "MeshData.h"
#include "ComponentData.h"
#include "ModelLayer.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <memory>

#include <spdlog/spdlog.h>

using namespace systems::io;
namespace fs = std::filesystem;

static bool approx_eq(double a, double b, double eps = 1e-9)
{
    return std::fabs(a - b) <= eps;
}

static void require_vertices_equal(const MeshData& mesh, const std::vector<std::array<double, 3>>& global_points)
{
    const std::vector<std::array<double, 3>> expected = {
        { -0.5, -0.5, 1.0 },
        { -0.5, 0.5, 1.0 },
        { -0.5, -0.5, 0.0 },
        { -0.5, 0.5, 0.0 },
        { 0.5, -0.5, 1.0 },
        { 0.5, 0.5, 1.0 },
        { 0.5, -0.5, 0.0 },
        { 0.5, 0.5, 0.0 },
        { 1.0, 1.1, 0.0 },
        { 1.1, 1.0, 0.0 },
        { 1.0, 1.0, 2.0 }
    };

    REQUIRE(mesh.vertex_count_ == static_cast<Index>(expected.size()));

    bool use_global_points = mesh.vertex_positions_.empty() && !mesh.local_to_global_.empty();

    for (Index i = 0; i < mesh.vertex_count_; ++i) {
        const Index gid = use_global_points ? mesh.local_to_global_[static_cast<size_t>(i)] : i;
        const auto& v = use_global_points ? global_points[static_cast<size_t>(gid)] : mesh.vertex_positions_[static_cast<size_t>(i)];
        REQUIRE(approx_eq(v[0], expected[i][0]));
        REQUIRE(approx_eq(v[1], expected[i][1]));
        REQUIRE(approx_eq(v[2], expected[i][2]));
    }
}

static void require_solid_element_matches_test_inp(const MeshData& mesh)
{
    const std::vector<Index> expected_indices = { 4, 5, 7, 6, 0, 1, 3, 2 };

    REQUIRE(mesh.solid_vertices_offset_.size() >= 2);
    const Index start = mesh.solid_vertices_offset_[0];
    const Index end = mesh.solid_vertices_offset_[1];
    REQUIRE(end - start == (Index)expected_indices.size());

    for (size_t i = 0; i < expected_indices.size(); ++i) {
        REQUIRE(mesh.solid_vertices_[(size_t)start + i] == expected_indices[i]);
    }
}

static bool meshes_equal(const MeshData& A, const MeshData& B, double eps = 1e-6)
{
    if (A.vertex_count_ != B.vertex_count_) {
        spdlog::error("顶点数不一致！");
        return false;
    }

    if (A.solid_types_.size() != B.solid_types_.size()) {
        spdlog::error("体单元数量不一致！");
        return false;
    }

    for (size_t i = 0; i < A.solid_types_.size(); ++i) {
        if (A.solid_types_[i] != B.solid_types_[i]) {
            spdlog::error("solid_types不一致！");
            return false;
        }
    }

    if (A.solid_vertices_offset_.size() != B.solid_vertices_offset_.size()) {
        spdlog::error("solid_vertices_offset不一致！");
        return false;
    }

    for (size_t i = 0; i < A.solid_vertices_offset_.size(); ++i) {
        if (A.solid_vertices_offset_[i] != B.solid_vertices_offset_[i]) {
            spdlog::error("solid_vertices_offset不一致！");
            return false;
        }
    }

    if (A.solid_vertices_.size() != B.solid_vertices_.size()) {
        spdlog::error("solid_vertices_.size不一致！");
        return false;
    }

    for (size_t i = 0; i < A.solid_vertices_.size(); ++i) {
        if (A.solid_vertices_[i] != B.solid_vertices_[i]) {
            spdlog::error("solid_vertices不一致！");
            return false;
        }
    }

    if (A.face_vertices_offset_.size() != B.face_vertices_offset_.size()) {
        spdlog::error("面顶点数不一致！");
        return false;
    }
    for (size_t i = 0; i < A.face_vertices_offset_.size(); ++i) {
        if (A.face_vertices_offset_[i] != B.face_vertices_offset_[i]) {
            spdlog::error("面的数量不一致！");
            return false;
        }
    }

    if (A.face_vertices_.size() != B.face_vertices_.size()) {
        spdlog::error("面顶点索引数量不一致！");
        return false;
    }

    for (size_t i = 0; i < A.face_vertices_.size(); ++i) {
        if (A.face_vertices_[i] != B.face_vertices_[i]) {
            spdlog::error("面顶点索引不一致！");
            return false;
        }
    }

    if (A.patches_.size() != B.patches_.size()) {
        spdlog::error("patch数量不一致！");
        return false;
    }

    if (A.blocks_.size() != B.blocks_.size()) {
        spdlog::error("block数量不一致！");
        return false;
    }

    return true;
}

static fs::path test_dir_from_source()
{
    fs::path p = fs::path(__FILE__).parent_path();
    return p;
}

static void require_vertices_equal_for_allfile(const MeshData& mesh, const std::vector<std::array<double, 3>>& global_points, fs::path infile)
{
    REQUIRE(fs::exists(infile));

    std::ifstream ifs(infile);
    REQUIRE(ifs.is_open());

    std::vector<std::array<double, 3>> expected;
    std::string line;
    bool in_node_block = false;
    while (std::getline(ifs, line)) {
        size_t pos = line.find_first_not_of(" \t\r\n");
        if (pos == std::string::npos)
            continue;
        std::string trimmed = line.substr(pos);

        if (!in_node_block) {
            if (trimmed.size() >= 5 && (trimmed.rfind("*NODE", 0) == 0 || trimmed.rfind("*node", 0) == 0)) {
                in_node_block = true;
            }
            continue;
        }

        if (!trimmed.empty() && trimmed[0] == '*')
            break;

        if (!trimmed.empty() && trimmed.size() >= 2 && trimmed[0] == '*' && trimmed[1] == '*')
            continue;

        std::vector<std::string> parts;
        size_t start = 0;
        while (start < trimmed.size()) {
            size_t comma = trimmed.find(',', start);
            std::string token;
            if (comma == std::string::npos) {
                token = trimmed.substr(start);
                start = trimmed.size();
            } else {
                token = trimmed.substr(start, comma - start);
                start = comma + 1;
            }
            size_t a = token.find_first_not_of(" \t\r\n");
            if (a == std::string::npos) {
                parts.push_back("");
                continue;
            }
            size_t b = token.find_last_not_of(" \t\r\n");
            parts.push_back(token.substr(a, b - a + 1));
        }

        if (parts.size() < 4)
            continue;

        try {
            double x = std::stod(parts[1]);
            double y = std::stod(parts[2]);
            double z = std::stod(parts[3]);
            expected.push_back({ x, y, z });
        } catch (...) {
            continue;
        }
    }

    REQUIRE(!expected.empty());
    REQUIRE(mesh.vertex_count_ == static_cast<Index>(expected.size()));

    bool use_global_points = mesh.vertex_positions_.empty() && !mesh.local_to_global_.empty();

    for (Index i = 0; i < mesh.vertex_count_; ++i) {
        const Index gid = use_global_points ? mesh.local_to_global_[static_cast<size_t>(i)] : i;
        const auto& v = use_global_points ? global_points[static_cast<size_t>(gid)] : mesh.vertex_positions_[static_cast<size_t>(i)];
        REQUIRE(approx_eq(v[0], expected[i][0]));
        REQUIRE(approx_eq(v[1], expected[i][1]));
        REQUIRE(approx_eq(v[2], expected[i][2]));
    }
}

TEST_CASE("InpModelHandler plugin Read test.inp ")
{
    spdlog::info("开始测试test.inp");
    InpModelHandler handler;
    fs::path dir = test_dir_from_source();
    fs::path infile = dir / "test.inp";
    REQUIRE(fs::exists(infile));

    auto payload_in = handler.read_model(infile, {});
    REQUIRE(payload_in.has_value());
    REQUIRE(!payload_in->components.empty());
    spdlog::info("read_model success, components.size()={}", payload_in->components.size());

    fs::path outfile = dir / "test_out.inp";
    if (fs::exists(outfile))
        fs::remove(outfile);

    ModelLayer mgr;
    mgr.addModel(payload_in->model_name, std::move(payload_in->components));
    spdlog::info("addModel success");
    
    ComponentData* comp_in = mgr.findComponent(0);
    REQUIRE(comp_in != nullptr);
    spdlog::info("findComponent(0) success");
    
    REQUIRE(comp_in->hasMesh());
    const MeshData* mesh = comp_in->asMeshData();
    REQUIRE(mesh != nullptr);
    spdlog::info("asMeshData success, vertex_count={}, local_to_global_.size()={}", 
        mesh->vertex_count_, mesh->local_to_global_.size());

    require_vertices_equal(*mesh, mgr.globalPoints());

    if (mesh->solid_vertices_offset_.size() >= 2 && (mesh->solid_vertices_offset_[1] - mesh->solid_vertices_offset_[0]) == 8) {
        require_solid_element_matches_test_inp(*mesh);
    } else {
        spdlog::error("期望存在一个8节点体元或转换未能填充 solid_vertices_ / solid_vertices_offset_");
    }

    std::vector<Index> component_ids = { 0 };
    handler.write_components(mgr, component_ids, outfile, {});
    REQUIRE(fs::exists(outfile));

    auto payload_out = handler.read_model(outfile, {});
    REQUIRE(payload_out.has_value());
    REQUIRE(!payload_out->components.empty());

    ComponentData* comp_out = payload_out->components[0].get();
    REQUIRE(comp_out != nullptr);
    REQUIRE(comp_out->hasMesh());

    const auto* mesh_out = comp_out->asMeshData();
    REQUIRE(mesh_out != nullptr);

    require_vertices_equal(*mesh_out, mgr.globalPoints());

    if (mesh_out->solid_vertices_offset_.size() >= 2 && (mesh_out->solid_vertices_offset_[1] - mesh_out->solid_vertices_offset_[0]) == 8) {
        require_solid_element_matches_test_inp(*mesh_out);
    } else {
        spdlog::error("round-trip 后未能保留 8 节点体元信息");
    }

    REQUIRE(meshes_equal(*mesh, *mesh_out));

    std::error_code ec;
    fs::remove(outfile, ec);
}

TEST_CASE("InpModelHandler ReadWrite test2.inp ")
{
    spdlog::debug("开始测试test2.inp");
    InpModelHandler handler;
    fs::path dir = test_dir_from_source();
    fs::path infile = dir / "test2.inp";
    REQUIRE(fs::exists(infile));

    auto payload_in = handler.read_model(infile, {});
    REQUIRE(payload_in.has_value());
    REQUIRE(!payload_in->components.empty());

    fs::path outfile = dir / "test2_out.inp";
    if (fs::exists(outfile))
        fs::remove(outfile);

    ModelLayer mgr;
    mgr.addModel(payload_in->model_name, std::move(payload_in->components));
    
    ComponentData* comp_in = mgr.findComponent(0);
    REQUIRE(comp_in != nullptr);
    REQUIRE(comp_in->hasMesh());
    const MeshData* mesh_in = comp_in->asMeshData();
    REQUIRE(mesh_in != nullptr);

    std::vector<Index> component_ids = { 0 };
    handler.write_components(mgr, component_ids, outfile, {});
    REQUIRE(fs::exists(outfile));

    auto payload_out = handler.read_model(outfile, {});
    REQUIRE(payload_out.has_value());
    REQUIRE(!payload_out->components.empty());

    ComponentData* comp_out = payload_out->components[0].get();
    REQUIRE(comp_out != nullptr);
    REQUIRE(comp_out->hasMesh());
    const MeshData* mesh_out = comp_out->asMeshData();
    REQUIRE(mesh_out != nullptr);

    REQUIRE(meshes_equal(*mesh_in, *mesh_out));

    std::error_code ec;
    fs::remove(outfile, ec);
}

TEST_CASE("InpModelHandler ReadWrite yuan.inp ")
{
    spdlog::debug("开始测试yuan.inp");
    InpModelHandler handler;
    fs::path dir = test_dir_from_source();
    fs::path infile = dir / "yuan.inp";
    REQUIRE(fs::exists(infile));

    auto payload_in = handler.read_model(infile, {});
    REQUIRE(payload_in.has_value());
    REQUIRE(!payload_in->components.empty());

    fs::path outfile = dir / "yuan_out.inp";
    if (fs::exists(outfile))
        fs::remove(outfile);

    ModelLayer mgr;
    mgr.addModel(payload_in->model_name, std::move(payload_in->components));
    
    ComponentData* comp_in = mgr.findComponent(0);
    REQUIRE(comp_in != nullptr);
    REQUIRE(comp_in->hasMesh());
    const MeshData* mesh_in = comp_in->asMeshData();
    REQUIRE(mesh_in != nullptr);

    std::vector<Index> component_ids = { 0 };
    handler.write_components(mgr, component_ids, outfile, {});
    REQUIRE(fs::exists(outfile));

    auto payload_out = handler.read_model(outfile, {});
    REQUIRE(payload_out.has_value());
    REQUIRE(!payload_out->components.empty());

    ComponentData* comp_out = payload_out->components[0].get();
    REQUIRE(comp_out != nullptr);
    REQUIRE(comp_out->hasMesh());
    const MeshData* mesh_out = comp_out->asMeshData();
    REQUIRE(mesh_out != nullptr);

    REQUIRE(meshes_equal(*mesh_in, *mesh_out));

    std::error_code ec;
    fs::remove(outfile, ec);
}

TEST_CASE("InpModelHandler ReadWrite wangGe4D.inp")
{
    spdlog::debug("开始测试wangGe4D.inp");
    InpModelHandler handler;
    fs::path dir = test_dir_from_source();
    fs::path infile = dir / "wangGe4D.inp";
    REQUIRE(fs::exists(infile));

    auto payload_in = handler.read_model(infile, {});
    REQUIRE(payload_in.has_value());
    REQUIRE(!payload_in->components.empty());

    fs::path outfile = dir / "wangGe4D_out.inp";
    if (fs::exists(outfile))
        fs::remove(outfile);

    ModelLayer mgr;
    mgr.addModel(payload_in->model_name, std::move(payload_in->components));
    
    ComponentData* comp_in = mgr.findComponent(0);
    REQUIRE(comp_in != nullptr);
    REQUIRE(comp_in->hasMesh());
    const MeshData* mesh_in = comp_in->asMeshData();
    REQUIRE(mesh_in != nullptr);

    std::vector<Index> component_ids = { 0 };
    handler.write_components(mgr, component_ids, outfile, {});
    REQUIRE(fs::exists(outfile));

    auto payload_out = handler.read_model(outfile, {});
    REQUIRE(payload_out.has_value());
    REQUIRE(!payload_out->components.empty());

    ComponentData* comp_out = payload_out->components[0].get();
    REQUIRE(comp_out != nullptr);
    REQUIRE(comp_out->hasMesh());
    const MeshData* mesh_out = comp_out->asMeshData();
    REQUIRE(mesh_out != nullptr);

    REQUIRE(meshes_equal(*mesh_in, *mesh_out));

    std::error_code ec;
    fs::remove(outfile, ec);
}

TEST_CASE("InpModelHandler ReadWrite aTest.inp")
{
    spdlog::debug("开始测试aTest.inp");
    InpModelHandler handler;
    fs::path dir = test_dir_from_source();
    fs::path infile = dir / "aTest.inp";
    REQUIRE(fs::exists(infile));

    auto payload_in = handler.read_model(infile, {});
    REQUIRE(payload_in.has_value());
    REQUIRE(!payload_in->components.empty());

    fs::path outfile = dir / "aTest_out.inp";
    if (fs::exists(outfile))
        fs::remove(outfile);

    ModelLayer mgr;
    mgr.addModel(payload_in->model_name, std::move(payload_in->components));
    
    ComponentData* comp_in = mgr.findComponent(0);
    REQUIRE(comp_in != nullptr);
    REQUIRE(comp_in->hasMesh());
    const MeshData* mesh_in = comp_in->asMeshData();
    REQUIRE(mesh_in != nullptr);

    std::vector<Index> component_ids = { 0 };
    handler.write_components(mgr, component_ids, outfile, {});
    REQUIRE(fs::exists(outfile));

    auto payload_out = handler.read_model(outfile, {});
    REQUIRE(payload_out.has_value());
    REQUIRE(!payload_out->components.empty());

    ComponentData* comp_out = payload_out->components[0].get();
    REQUIRE(comp_out != nullptr);
    REQUIRE(comp_out->hasMesh());
    const MeshData* mesh_out = comp_out->asMeshData();
    REQUIRE(mesh_out != nullptr);

    REQUIRE(meshes_equal(*mesh_in, *mesh_out));

    std::error_code ec;
    fs::remove(outfile, ec);
}

TEST_CASE("InpModelHandler ReadWrite allQuardLuoShuan.inp")
{
    spdlog::debug("开始测试allQuardLuoShuan.inp");
    InpModelHandler handler;
    fs::path dir = test_dir_from_source();
    fs::path infile = dir / "allQuardLuoShuan.inp";
    REQUIRE(fs::exists(infile));

    auto payload_in = handler.read_model(infile, {});
    REQUIRE(payload_in.has_value());
    REQUIRE(!payload_in->components.empty());

    fs::path outfile = dir / "allQuardLuoShuan_out.inp";
    if (fs::exists(outfile))
        fs::remove(outfile);

    ModelLayer mgr;
    mgr.addModel(payload_in->model_name, std::move(payload_in->components));
    
    ComponentData* comp_in = mgr.findComponent(0);
    REQUIRE(comp_in != nullptr);
    REQUIRE(comp_in->hasMesh());
    const MeshData* mesh_in = comp_in->asMeshData();
    REQUIRE(mesh_in != nullptr);

    std::vector<Index> component_ids = { 0 };
    handler.write_components(mgr, component_ids, outfile, {});
    REQUIRE(fs::exists(outfile));

    auto payload_out = handler.read_model(outfile, {});
    REQUIRE(payload_out.has_value());
    REQUIRE(!payload_out->components.empty());

    ComponentData* comp_out = payload_out->components[0].get();
    REQUIRE(comp_out != nullptr);
    REQUIRE(comp_out->hasMesh());
    const MeshData* mesh_out = comp_out->asMeshData();
    REQUIRE(mesh_out != nullptr);

    REQUIRE(meshes_equal(*mesh_in, *mesh_out));

    std::error_code ec;
    fs::remove(outfile, ec);
}

TEST_CASE("InpModelHandler ReadWrite MixedTi_LuoShuan.inp")
{
    spdlog::debug("开始测试MixedTi_LuoShuan.inp");
    InpModelHandler handler;
    fs::path dir = test_dir_from_source();
    fs::path infile = dir / "MixedTi_LuoShuan.inp";
    REQUIRE(fs::exists(infile));

    auto payload_in = handler.read_model(infile, {});
    REQUIRE(payload_in.has_value());
    REQUIRE(!payload_in->components.empty());

    fs::path outfile = dir / "MixedTi_LuoShuan_out.inp";
    if (fs::exists(outfile))
        fs::remove(outfile);

    ModelLayer mgr;
    mgr.addModel(payload_in->model_name, std::move(payload_in->components));
    
    ComponentData* comp_in = mgr.findComponent(0);
    REQUIRE(comp_in != nullptr);
    REQUIRE(comp_in->hasMesh());
    const MeshData* mesh_in = comp_in->asMeshData();
    REQUIRE(mesh_in != nullptr);

    require_vertices_equal_for_allfile(*mesh_in, mgr.globalPoints(), infile);

    std::vector<Index> component_ids = { 0 };
    handler.write_components(mgr, component_ids, outfile, {});
    REQUIRE(fs::exists(outfile));

    auto payload_out = handler.read_model(outfile, {});
    REQUIRE(payload_out.has_value());
    REQUIRE(!payload_out->components.empty());

    ComponentData* comp_out = payload_out->components[0].get();
    REQUIRE(comp_out != nullptr);
    REQUIRE(comp_out->hasMesh());
    const MeshData* mesh_out = comp_out->asMeshData();
    REQUIRE(mesh_out != nullptr);

    REQUIRE(meshes_equal(*mesh_in, *mesh_out));

    std::error_code ec;
    fs::remove(outfile, ec);
}
