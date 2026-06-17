/**
 * @file TestMModelHandler.cpp
 * @brief M (MeshLib CTMesh) 模型文件处理器单元测试
 *
 * MModelHandler 用 MeshLib::CTMesh 读写 .m 文件，当前写出入口为组件化
 * write_components()。源 MeshData 需要先进入 ModelLayer，由 ModelLayer 维护全局点池
 * 与组件 MeshData 的 local_to_global_/vertex_count_，再交给 MModelHandler 导出。
 * .m 格式以表面三角网格为主，不支持体单元，写出后体信息不会回流。
 */
#include "MModelHandler.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelLayer.h"
#include "TempFile.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <memory>
#include <vector>

namespace fs = std::filesystem;

namespace {
/**
 * @brief 构造一个仅含表面三角形的最小 MeshData（含 patch/block）
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

/**
 * @brief 构造含两个 Patch / 两个 Block 的 MeshData
 */
MeshData MakeMultiPatchMesh()
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

    // Patch 1 -> Block 1，前两个面
    auto patch1 = std::make_unique<Patch>(1, 1);
    patch1->faces = { 0, 1 };
    m.patches_[1] = std::move(patch1);

    auto block1 = std::make_unique<Block>();
    block1->id = 1;
    block1->patchIDs = { 1 };
    m.blocks_[1] = std::move(block1);

    // Patch 2 -> Block 2，后两个面
    auto patch2 = std::make_unique<Patch>(2, 2);
    patch2->faces = { 2, 3 };
    m.patches_[2] = std::move(patch2);

    auto block2 = std::make_unique<Block>();
    block2->id = 2;
    block2->patchIDs = { 2 };
    m.blocks_[2] = std::move(block2);

    return m;
}

std::vector<Index> addMeshModelAndGetComponentIds(
    ModelLayer& layer,
    std::unique_ptr<MeshData> mesh)
{
    auto model = std::make_unique<ModelData>(std::move(mesh));
    const Index modelId = layer.addModel(std::move(model));
    REQUIRE(modelId >= 0);

    std::vector<Index> componentIds = layer.getComponentIds(modelId);
    REQUIRE(!componentIds.empty());

    return componentIds;
}

const MeshData* requireReadableMeshModel(const ModelData& model)
{
    const auto& components = model.stagingcomponents();
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

    std::unique_ptr<ModelData> read_back;
    REQUIRE_NOTHROW(read_back = io.read_model(out, {}));
    REQUIRE(read_back != nullptr);

    const MeshData* read_mesh = requireReadableMeshModel(*read_back);

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

    // .m 格式内部经 CTMesh 转换，patch / block ID 不保证原样保留，
    // 但面数据应完整回流，且至少存在一个 patch 包含所有面。
    size_t total_faces = 0;
    for (const auto& [pid, patch] : read_mesh->patches_) {
        total_faces += patch->faces.size();
    }
    REQUIRE(total_faces == ref.face_vertices_offset_.size() - 1);
    REQUIRE(!read_mesh->patches_.empty());
}

TEST_CASE("MModelHandler::multi-patch round-trip")
{
    systems::io::MModelHandler io;
    fs::path out = core::TempFile::instance().path().string() + "_multi.m";

    ModelLayer layer;
    std::vector<Index> componentIds = addMeshModelAndGetComponentIds(
        layer,
        std::make_unique<MeshData>(MakeMultiPatchMesh()));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    std::unique_ptr<ModelData> read_back;
    REQUIRE_NOTHROW(read_back = io.read_model(out, {}));
    REQUIRE(read_back != nullptr);

    const MeshData* read_mesh = requireReadableMeshModel(*read_back);

    const MeshData ref = MakeMultiPatchMesh();
    REQUIRE(read_mesh->vertex_positions_.size() == ref.vertex_positions_.size());
    REQUIRE(read_mesh->face_vertices_offset_.size() == ref.face_vertices_offset_.size());

    // .m 内部经 CTMesh 转换，patch / block 分组信息可能合并或丢失，
    // 但面数据必须完整保留。
    size_t total_faces = 0;
    for (const auto& [pid, patch] : read_mesh->patches_) {
        total_faces += patch->faces.size();
    }
    REQUIRE(total_faces == ref.face_vertices_offset_.size() - 1);
    REQUIRE(!read_mesh->patches_.empty());
}

TEST_CASE("MModelHandler::write_components() without patches produces empty file")
{
    systems::io::MModelHandler io;
    fs::path out = core::TempFile::instance().path().string() + "_empty.m";

    MeshData m;
    m.init();
    m.vertex_positions_ = { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 } };
    m.face_vertices_ = { 0, 1, 2 };
    m.face_vertices_offset_ = { 0, 3 };
    // 注意：没有 patches_

    ModelLayer layer;
    std::vector<Index> componentIds = addMeshModelAndGetComponentIds(
        layer,
        std::make_unique<MeshData>(std::move(m)));

    REQUIRE_NOTHROW(io.write_components(layer, componentIds, out, {}));
    REQUIRE(fs::exists(out));

    // 无 patch 时读回应为空或基本空网格
    std::unique_ptr<ModelData> read_back;
    REQUIRE_NOTHROW(read_back = io.read_model(out, {}));
    REQUIRE(read_back != nullptr);

    const MeshData* read_mesh = requireReadableMeshModel(*read_back);
    // 读回的面数应为 0（CTMesh 读 .m 后无面）
    REQUIRE(read_mesh->face_vertices_offset_.size() <= 1); // 只有 {0} 或空
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

    std::unique_ptr<ModelData> read_back;
    REQUIRE_NOTHROW(read_back = io.read_model(out, {}));
    REQUIRE(read_back != nullptr);
    REQUIRE(read_back->model_name_ == out.filename().string());
}