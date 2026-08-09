/**
 * @file TestOBJModelHandler.cpp
 * @brief OBJ 模型文件处理器单元测试
 *
 * 由于 OBJ 仅支持表面三角/多边形网格（不支持体单元 solid_*），
 * 这里不直接复用 MakeMeshData()（其 solid 部分会在 OBJ 写出/读回过程中丢失），
 * 而是构造一个仅含点 + 面的最小网格，做读写回环验证。
 */
#include "ComponentData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelPayload.h"
#include "OBJModelHandler.h"
#include "TempFile.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <vector>

namespace fs = std::filesystem;

namespace {
/**
 * @brief 构造一个仅含表面三角形的最小 MeshData
 *
 * - 4 个顶点（一个四面体的 4 个角点）
 * - 4 个三角形面
 */
MeshData MakeSurfaceMesh()
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
        0, 1, 2,
        0, 1, 3,
        0, 2, 3,
        1, 2, 3,
    };
    m.face_vertices_offset_ = { 0, 3, 6, 9, 12 };

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

TEST_CASE("OBJModelHandler::write_components()/read_model() round-trip")
{
    systems::io::OBJModelHandler io;

    fs::path out;
    SECTION("Latin path")
    {
        out = core::TempFile::instance().path().string() + ".obj";
    }
    SECTION("Chinese filename")
    {
        out = core::TempFile::instance().path();
        out.replace_filename("中文_" + out.stem().string() + ".obj");
    }

    // MeshData 含 unique_ptr，不可拷贝；用两份独立构造的源做对比基准
    ModelLayer layer;
    std::vector<Index> componentIds = addMeshModelAndGetComponentIds(
        layer,
        std::make_unique<MeshData>(MakeSurfaceMesh()));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());

    const MeshData* read_mesh = requireReadableMeshModel(*payload);

    const MeshData ref = MakeSurfaceMesh();
    // 顶点数应一致
    REQUIRE(read_mesh->vertex_positions_.size() == ref.vertex_positions_.size());
    // 面数应一致（offset 元素数 = 面数 + 1）
    REQUIRE(read_mesh->face_vertices_offset_.size() == ref.face_vertices_offset_.size());
    // 面顶点索引总数应一致
    REQUIRE(read_mesh->face_vertices_.size() == ref.face_vertices_.size());

    // 验证顶点坐标精确写回（OBJ 是文本格式，浮点应可精确恢复到很小误差）
    for (size_t i = 0; i < ref.vertex_positions_.size(); ++i) {
        for (int k = 0; k < 3; ++k) {
            REQUIRE(read_mesh->vertex_positions_[i][k] == ref.vertex_positions_[i][k]);
        }
    }
}

TEST_CASE("OBJModelHandler::read_model() - model_name preserved")
{
    systems::io::OBJModelHandler io;
    fs::path out = core::TempFile::instance().path().string() + "_name.obj";

    ModelLayer layer;
    std::vector<Index> componentIds = addMeshModelAndGetComponentIds(
        layer,
        std::make_unique<MeshData>(MakeSurfaceMesh()));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(out, {}));
    REQUIRE(payload.has_value());
    REQUIRE(payload->model_name == out.filename().string());
}

TEST_CASE("OBJModelHandler::read_model() - shapes split into components")
{
    // 按 shape(group) 拆分：每个组读为一个独立组件，MeshData 自包含（点索引重映射）
    systems::io::OBJModelHandler io;
    fs::path in = core::TempFile::instance().path().string() + "_groups.obj";

    {
        std::ofstream ofs(in);
        ofs << "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 0 0 1\n";
        ofs << "g first\nf 1 2 3\n";
        ofs << "g second\nf 2 3 4\nf 1 2 4\n";
    }

    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = io.read_model(in, {}));
    REQUIRE(payload.has_value());
    REQUIRE(payload->components.size() == 2);

    const auto& first = payload->components[0];
    const auto& second = payload->components[1];
    REQUIRE(first->name == "first");
    REQUIRE(second->name == "second");

    // first：1 个面、3 个顶点；second：2 个面、4 个顶点（均为组件内局部点索引）
    REQUIRE(first->mesh->face_vertices_offset_.size() == 2);
    REQUIRE(first->mesh->vertex_count_ == 3);
    REQUIRE(second->mesh->face_vertices_offset_.size() == 3);
    REQUIRE(second->mesh->vertex_count_ == 4);
}

TEST_CASE("OBJModelHandler::write_components() - empty MeshData gracefully")
{
    // OBJ 处理器仅支持有效 MeshData。空 MeshData 应记录错误，但不应崩溃。
    systems::io::OBJModelHandler io;
    fs::path out = core::TempFile::instance().path().string() + "_empty.obj";

    auto mesh_data = std::make_unique<MeshData>();
    mesh_data->init();

    ModelLayer layer;
    std::vector<Index> componentIds = addMeshModelAndGetComponentIds(layer, std::move(mesh_data));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
}
