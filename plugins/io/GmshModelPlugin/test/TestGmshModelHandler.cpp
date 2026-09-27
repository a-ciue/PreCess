/**
 * @file TestGmshModelHandler.cpp
 * @brief GmshModelHandler 的单元测试：2.2/4.1 解析、单元类型映射、错误处理与合并导出
 */
#include "ComponentData.h"
#include "GmshModelHandler.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelPayload.h"
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

//! @brief msh 2.2：4 点(非连续 tag) + 1 三角形 + 1 四边形 + 1 四面体 + 1 线段
const char* kMsh2_2 = "$MeshFormat\r\n"
                      "2.2 0 8\r\n"
                      "$EndMeshFormat\r\n"
                      "$Nodes\r\n"
                      "5\r\n"
                      "3 0 0 0\r\n"
                      "1 1 0 0\r\n"
                      "4 1 1 0\r\n"
                      "7 0 1 0\r\n"
                      "9 0 0 1\r\n"
                      "$EndNodes\r\n"
                      "$Elements\r\n"
                      "4\r\n"
                      "1 2 2 0 0 3 1 4\r\n"
                      "2 3 2 0 0 3 1 4 7\r\n"
                      "3 4 2 0 0 3 1 4 9\r\n"
                      "4 1 0 3 1\r\n"
                      "$EndElements\r\n";

//! @brief msh 4.1：两个 block（2D 三角形用 tag 1..4，3D 四面体用 tag 5..8），
//! tag 段与坐标段均为每行一条（4.1 规范布局）
const char* kMsh4_1 = "$MeshFormat\r\n"
                      "4.1 0 8\r\n"
                      "$EndMeshFormat\r\n"
                      "$Entities\r\n"
                      "0 0 1 1\r\n"
                      "$EndEntities\r\n"
                      "$Nodes\r\n"
                      "2 8 1 8\r\n"
                      "2 1 0 4\r\n"
                      "1\r\n"
                      "2\r\n"
                      "3\r\n"
                      "4\r\n"
                      "0 0 0\r\n"
                      "1 0 0\r\n"
                      "1 1 0\r\n"
                      "0 1 0\r\n"
                      "3 2 0 4\r\n"
                      "5\r\n"
                      "6\r\n"
                      "7\r\n"
                      "8\r\n"
                      "0 0 1\r\n"
                      "1 0 1\r\n"
                      "0 1 1\r\n"
                      "1 1 1\r\n"
                      "$EndNodes\r\n"
                      "$Elements\r\n"
                      "2 2 1 2\r\n"
                      "2 1 2 1\r\n"
                      "1 1 2 3\r\n"
                      "3 2 4 1\r\n"
                      "2 5 6 7 8\r\n"
                      "$EndElements\r\n";

//! @brief 写出一份测试用文本文件
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

//! @brief 单个四面体 + 一个三角形面 + 一条边的混合网格
MeshData makeMixedMesh()
{
    MeshData mesh;
    mesh.init();
    mesh.vertex_positions_ = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 },
    };
    mesh.vertex_count_ = 4;
    mesh.edge_vertices_ = { 0, 1 };
    mesh.face_vertices_ = { 0, 1, 2 };
    mesh.face_vertices_offset_ = { 0, 3 };
    mesh.solid_types_ = { 10 }; // VTK_TETRA
    mesh.solid_vertices_ = { 0, 1, 2, 3 };
    mesh.solid_vertices_offset_ = { 0, 4 };
    mesh.solid_faces_offset_ = { 0 };
    return mesh;
}

/**
 * @brief 读回文件并返回 payload 本体
 *
 * 按值返回：组件由 unique_ptr 持有，返回裸指针会随局部 payload 析构而悬空。
 */
ModelPayload requireReadPayload(systems::io::GmshModelHandler& handler, const fs::path& path)
{
    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = handler.read_model(path, {}));
    REQUIRE(payload.has_value());
    REQUIRE(!payload->components.empty());
    REQUIRE(payload->components.front()->mesh);
    return std::move(*payload);
}
} // namespace

TEST_CASE("GmshModelHandler reads msh 2.2 with arbitrary node tags")
{
    systems::io::GmshModelHandler handler;
    const fs::path input = core::TempFile::instance().path().string() + "_read22.msh";

    writeFile(input, kMsh2_2);
    const ModelPayload payload = requireReadPayload(handler, input);
    const MeshData* mesh = payload.components.front()->mesh.get();

    // 文件 tag 3,1,4,7,9 按出现顺序换算为局部点 0..4
    REQUIRE(mesh->vertex_count_ == 5);
    REQUIRE(mesh->vertex_positions_[1] == std::array<double, 3> { 1.0, 0.0, 0.0 });

    // 三角形 3 1 4 -> 局部 0 1 2；四边形 3 1 4 7 -> 0 1 2 3
    REQUIRE(mesh->face_vertices_ == std::vector<Index> { 0, 1, 2, 0, 1, 2, 3 });
    REQUIRE(mesh->face_vertices_offset_ == std::vector<Index> { 0, 3, 7 });

    // 四面体 3 1 4 9 -> 局部 0 1 2 4，VTK_TETRA
    REQUIRE(mesh->solid_types_ == std::vector<unsigned char> { 10 });
    REQUIRE(mesh->solid_vertices_ == std::vector<Index> { 0, 1, 2, 4 });
    REQUIRE(mesh->solid_vertices_offset_ == std::vector<Index> { 0, 4 });

    // 线段 3 1 -> 局部 0 1
    REQUIRE(mesh->edge_vertices_ == std::vector<Index> { 0, 1 });
}

TEST_CASE("GmshModelHandler reads msh 4.1 with entity blocks")
{
    systems::io::GmshModelHandler handler;
    const fs::path input = core::TempFile::instance().path().string() + "_read41.msh";

    writeFile(input, kMsh4_1);
    const ModelPayload payload = requireReadPayload(handler, input);
    const MeshData* mesh = payload.components.front()->mesh.get();

    // 两个 block 共 8 点：block1 tag 1..4（局部 0..3），block2 tag 5..8（局部 4..7）
    REQUIRE(mesh->vertex_count_ == 8);
    REQUIRE(mesh->vertex_positions_[4] == std::array<double, 3> { 0.0, 0.0, 1.0 });

    // 三角形引用 block1 的点（局部 0..2），四面体引用 block2 的点（局部 4..7）
    REQUIRE(mesh->face_vertices_ == std::vector<Index> { 0, 1, 2 });
    REQUIRE(mesh->face_vertices_offset_ == std::vector<Index> { 0, 3 });
    REQUIRE(mesh->solid_types_ == std::vector<unsigned char> { 10 });
    REQUIRE(mesh->solid_vertices_ == std::vector<Index> { 4, 5, 6, 7 });
    REQUIRE(mesh->solid_vertices_offset_ == std::vector<Index> { 0, 4 });
}

TEST_CASE("GmshModelHandler rejects unsupported and corrupt files")
{
    systems::io::GmshModelHandler handler;

    SECTION("binary msh declared")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_bin.msh";
        writeFile(input, "$MeshFormat\n4.1 1 8\n$EndMeshFormat\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("unknown node tag reference")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_badref.msh";
        writeFile(input,
            "$MeshFormat\n2.2 0 8\n$EndMeshFormat\n"
            "$Nodes\n1\n1 0 0 0\n$EndNodes\n"
            "$Elements\n1\n1 2 2 0 0 1 2 3\n$EndElements\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("element node list truncated")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_short.msh";
        writeFile(input,
            "$MeshFormat\n2.2 0 8\n$EndMeshFormat\n"
            "$Nodes\n3\n1 0 0 0\n2 1 0 0\n3 0 1 0\n$EndNodes\n"
            "$Elements\n1\n1 2 0 1 2\n$EndElements\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("no element records")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_noelem.msh";
        writeFile(input,
            "$MeshFormat\n2.2 0 8\n$EndMeshFormat\n"
            "$Nodes\n1\n1 0 0 0\n$EndNodes\n"
            "$Elements\n0\n$EndElements\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("only point element records")
    {
        // 点单元（type 15）不装配：与写出侧 has_cells 口径对齐，
        // 只含跳过单元的文件整体失败而非产出无连通性的空网格
        const fs::path input = core::TempFile::instance().path().string() + "_pointsonly.msh";
        writeFile(input,
            "$MeshFormat\n2.2 0 8\n$EndMeshFormat\n"
            "$Nodes\n3\n1 0 0 0\n2 1 0 0\n3 0 1 0\n$EndNodes\n"
            "$Elements\n3\n"
            "1 15 2 0 0 1\n"
            "2 15 2 0 0 2\n"
            "3 15 2 0 0 3\n"
            "$EndElements\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("4.1 only point element block")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_pointsonly41.msh";
        writeFile(input,
            "$MeshFormat\n4.1 0 8\n$EndMeshFormat\n"
            "$Nodes\n1 3 1 3\n2 1 0 3\n1\n2\n3\n"
            "0 0 0\n1 0 0\n0 1 0\n$EndNodes\n"
            "$Elements\n1 3 1 3\n2 1 15 3\n1 1\n2 2\n3 3\n$EndElements\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("missing file")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_missing.msh";
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("2.2 declared node count exceeds file size")
    {
        // 天文数字计数须在分配前按文件大小上界拒绝，而非 length_error 逃逸
        const fs::path input = core::TempFile::instance().path().string() + "_huge22.msh";
        writeFile(input,
            "$MeshFormat\n2.2 0 8\n$EndMeshFormat\n"
            "$Nodes\n999999999999999\n$EndNodes\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("4.1 block entry count exceeds file size")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_huge41.msh";
        writeFile(input,
            "$MeshFormat\n4.1 0 8\n$EndMeshFormat\n"
            "$Nodes\n1 999999999999999 1 1\n2 1 0 999999999999999\n$EndNodes\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }
}

TEST_CASE("GmshModelHandler skips high-order elements but keeps the rest")
{
    systems::io::GmshModelHandler handler;
    const fs::path input = core::TempFile::instance().path().string() + "_highorder.msh";

    // type 9（二次四面体，10 节点）不在支持表中：告警跳过；type 2 三角形正常装配
    writeFile(input,
        "$MeshFormat\n2.2 0 8\n$EndMeshFormat\n"
        "$Nodes\n4\n1 0 0 0\n2 1 0 0\n3 0 1 0\n4 0 0 1\n$EndNodes\n"
        "$Elements\n2\n"
        "1 9 2 0 0 1 2 3 4 1 2 3 4 1\n"
        "2 2 2 0 0 1 2 3\n"
        "$EndElements\n");

    const ModelPayload payload = requireReadPayload(handler, input);
    const MeshData* mesh = payload.components.front()->mesh.get();
    REQUIRE(mesh->face_vertices_ == std::vector<Index> { 0, 1, 2 });
    REQUIRE(mesh->solid_types_.empty());
}

TEST_CASE("GmshModelHandler reads version 2/2.0 headers and skips post-view sections")
{
    systems::io::GmshModelHandler handler;

    // 真实生态两种 2.0 写法：FreeFEM 写 "2 0 8"、gmsh 写 "2.0 0 8"，布局与 2.2 一致；
    // 后视数据节（$NodeData 等）不是网格数据，按未知节整段跳过——同名节连续多段
    // （gmsh 官方示例 view5.msh 的形态）时每段都要执行跳段
    const std::string body = "$Nodes\n3\n1 0 0 0\n2 1 0 0\n3 0 1 0\n$EndNodes\n"
                             "$Elements\n2\n"
                             "1 2 2 0 0 1 2 3\n"
                             "2 15 3 0 2 0 2\n"
                             "$EndElements\n"
                             "$NodeData\n1\n\"field\"\n1\n0\n3\n1 0.1\n2 0.2\n3 0.3\n$EndNodeData\n"
                             "$NodeData\n1\n\"field2\"\n1\n0\n3\n1 0.4\n2 0.5\n3 0.6\n$EndNodeData\n";

    SECTION("integer version token '2 0 8'")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_v2int.msh";
        writeFile(input, "$MeshFormat\n2 0 8\n$EndMeshFormat\n" + body);
        const ModelPayload payload = requireReadPayload(handler, input);
        const MeshData* mesh = payload.components.front()->mesh.get();
        REQUIRE(mesh->vertex_count_ == 3);
        REQUIRE(mesh->face_vertices_ == std::vector<Index> { 0, 1, 2 });
    }

    SECTION("decimal version token '2.0 0 8'")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_v2dec.msh";
        writeFile(input, "$MeshFormat\n2.0 0 8\n$EndMeshFormat\n" + body);
        const ModelPayload payload = requireReadPayload(handler, input);
        const MeshData* mesh = payload.components.front()->mesh.get();
        REQUIRE(mesh->vertex_count_ == 3);
        REQUIRE(mesh->face_vertices_ == std::vector<Index> { 0, 1, 2 });
    }
}

TEST_CASE("GmshModelHandler write_components round-trips a mixed mesh")
{
    systems::io::GmshModelHandler handler;
    const fs::path out = core::TempFile::instance().path().string() + "_roundtrip.msh";

    ModelLayer layer;
    const std::vector<Index> component_ids = addMeshComponent(layer, std::make_unique<MeshData>(makeMixedMesh()));

    REQUIRE_NOTHROW(handler.write_components(layer, component_ids, out, {}));
    REQUIRE(fs::exists(out));

    // 写出为 2.2（tag 连续 1 基），读回后边/面/体三类单元完整还原
    const ModelPayload payload = requireReadPayload(handler, out);
    const MeshData* mesh = payload.components.front()->mesh.get();
    REQUIRE(mesh->vertex_count_ == 4);
    REQUIRE(mesh->edge_vertices_ == std::vector<Index> { 0, 1 });
    REQUIRE(mesh->face_vertices_offset_ == std::vector<Index> { 0, 3 });
    REQUIRE(mesh->solid_types_ == std::vector<unsigned char> { 10 });
    REQUIRE(mesh->solid_vertices_offset_ == std::vector<Index> { 0, 4 });
}

TEST_CASE("GmshModelHandler write_components merges components with point offset")
{
    systems::io::GmshModelHandler handler;
    const fs::path out = core::TempFile::instance().path().string() + "_merge.msh";

    // 两个四面体组件：合并导出后 8 点 2 个四面体
    ModelLayer layer;
    const std::vector<Index> first_ids = addMeshComponent(layer, std::make_unique<MeshData>(makeMixedMesh()));
    const std::vector<Index> second_ids = addMeshComponent(layer, std::make_unique<MeshData>(makeMixedMesh()));

    std::vector<Index> component_ids = first_ids;
    component_ids.insert(component_ids.end(), second_ids.begin(), second_ids.end());

    REQUIRE_NOTHROW(handler.write_components(layer, component_ids, out, {}));
    REQUIRE(fs::exists(out));

    const ModelPayload payload = requireReadPayload(handler, out);
    const MeshData* mesh = payload.components.front()->mesh.get();
    REQUIRE(mesh->vertex_count_ == 8);
    REQUIRE(mesh->solid_types_.size() == 2);
    // 第二个组件的四面体引用偏移后的点（4..7）
    REQUIRE(mesh->solid_vertices_[4] >= 4);
    REQUIRE(mesh->solid_vertices_.back() < 8);
}

TEST_CASE("GmshModelHandler write_components skips polygon faces and unsupported solids")
{
    systems::io::GmshModelHandler handler;
    const fs::path out = core::TempFile::instance().path().string() + "_poly.msh";

    // 一个五边形面 + 一个 VTK_POLYHEDRON 体：msh 无法承载，导出整体失败且不产生文件
    MeshData poly_only;
    poly_only.init();
    poly_only.vertex_positions_ = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 1.0, 1.0, 0.0 },
        { 0.5, 1.5, 0.0 },
        { 0.0, 1.0, 0.0 },
    };
    poly_only.vertex_count_ = 5;
    poly_only.face_vertices_ = { 0, 1, 2, 3, 4 };
    poly_only.face_vertices_offset_ = { 0, 5 };

    ModelLayer layer;
    const std::vector<Index> component_ids = addMeshComponent(layer, std::make_unique<MeshData>(std::move(poly_only)));

    REQUIRE_NOTHROW(handler.write_components(layer, component_ids, out, {}));
    REQUIRE_FALSE(fs::exists(out));
}
