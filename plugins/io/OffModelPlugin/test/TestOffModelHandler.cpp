/**
 * @file TestOffModelHandler.cpp
 * @brief OffModelHandler 的单元测试：多组件合并导出与脏面过滤
 */
#include "ComponentData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelPayload.h"
#include "OffModelHandler.h"
#include "TempFile.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {
//! @brief 构造一个仅含点与面的最小 MeshData（4 点 4 面四面体）
MeshData makeSurfaceMesh()
{
    MeshData mesh;
    mesh.init();
    mesh.vertex_positions_ = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 },
    };
    mesh.face_vertices_ = {
        0,
        1,
        2,
        0,
        1,
        3,
        0,
        2,
        3,
        1,
        2,
        3,
    };
    mesh.face_vertices_offset_ = { 0, 3, 6, 9, 12 };
    return mesh;
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
ModelPayload requireReadPayload(systems::io::OffModelHandler& handler, const fs::path& path)
{
    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = handler.read_model(path, {}));
    REQUIRE(payload.has_value());
    REQUIRE(!payload->components.empty());
    REQUIRE(payload->components.front()->mesh);
    return std::move(*payload);
}
} // namespace

TEST_CASE("OffModelHandler write_components merges components with point offset")
{
    systems::io::OffModelHandler handler;
    const fs::path out = core::TempFile::instance().path().string() + "_merge.off";

    // 两个相同四面体组件：合并后 8 点 8 面，第二个组件的面索引整体偏移 4
    ModelLayer layer;
    const std::vector<Index> first_ids = addMeshComponent(layer, std::make_unique<MeshData>(makeSurfaceMesh()));
    const std::vector<Index> second_ids = addMeshComponent(layer, std::make_unique<MeshData>(makeSurfaceMesh()));

    std::vector<Index> component_ids = first_ids;
    component_ids.insert(component_ids.end(), second_ids.begin(), second_ids.end());

    REQUIRE_NOTHROW(handler.write_components(layer, component_ids, out, {}));
    REQUIRE(fs::exists(out));

    const ModelPayload payload = requireReadPayload(handler, out);
    const MeshData* mesh = payload.components.front()->mesh.get();
    REQUIRE(mesh->vertex_positions_.size() == 8);
    REQUIRE(mesh->face_vertices_offset_.size() == 9); // 8 个面
    REQUIRE(mesh->face_vertices_.size() == 24);
    // 第二个组件的面引用偏移后的点（4..7）
    REQUIRE(mesh->face_vertices_[12] >= 4);
    REQUIRE(mesh->face_vertices_.back() < 8);
}

TEST_CASE("OffModelHandler write_components skips dirty and degenerate faces")
{
    // 回归：合并阶段逐点校验源面索引，点索引越界的脏面整面跳过，
    // 不把无效索引写入合并结果，也不因有符号溢出产生未定义行为。
    // 注意：入池（addModel -> adoptComponent）要求数据自洽——面索引必须指向存在的点，
    // 否则 gid/邻接构建会按下标访问越界；因此脏面在入池后注入，模拟运行期产生的不一致。
    systems::io::OffModelHandler handler;
    const fs::path out = core::TempFile::instance().path().string() + "_dirty.off";

    // 入池时用合法数据：3 点 1 面
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
    };
    mesh->face_vertices_ = { 0, 1, 2 };
    mesh->face_vertices_offset_ = { 0, 3 };

    ModelLayer layer;
    const std::vector<Index> component_ids = addMeshComponent(layer, std::move(mesh));

    // 入池后注入脏面：面1 点索引越界；面2 退化（2 点）；面3 offset 越界
    ComponentData* component = layer.findComponent(component_ids[0]);
    REQUIRE(component);
    REQUIRE(component->mesh);
    component->mesh->face_vertices_ = {
        0,
        1,
        2,
        0,
        1,
        7,
        1,
        2,
        0,
        1,
        2,
    };
    component->mesh->face_vertices_offset_ = { 0, 3, 6, 8, 12 };

    REQUIRE_NOTHROW(handler.write_components(layer, component_ids, out, {}));
    REQUIRE(fs::exists(out));

    const ModelPayload payload = requireReadPayload(handler, out);
    const MeshData* read_mesh = payload.components.front()->mesh.get();
    REQUIRE(read_mesh->vertex_positions_.size() == 3);
    REQUIRE(read_mesh->face_vertices_ == std::vector<Index> { 0, 1, 2 });
    REQUIRE(read_mesh->face_vertices_offset_ == std::vector<Index> { 0, 3 });
}
