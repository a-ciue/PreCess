/**
 * @file TestObjMeshIO.cpp
 * @brief ObjMeshIO 组件级读写（ComponentDatas）回环测试
 */
#include "ComponentData.h"
#include "MeshData.h"
#include "ObjMeshIO.h"
#include "TempFile.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <fstream>
#include <memory>

namespace {
//! @brief OBJ 导入应发布的面组号属性名。
constexpr char patch_attribute_name[] = "patch_";

std::unique_ptr<ComponentData> makeComponent(
    const std::string& name,
    std::vector<std::array<double, 3>> points,
    std::vector<Index> face_vertices,
    std::vector<Index> face_offsets)
{
    auto mesh = std::make_unique<MeshData>();
    mesh->init();
    mesh->vertex_positions_ = std::move(points);
    mesh->vertex_count_ = static_cast<Index>(mesh->vertex_positions_.size());
    mesh->face_vertices_ = std::move(face_vertices);
    mesh->face_vertices_offset_ = std::move(face_offsets);

    auto component = std::make_unique<ComponentData>();
    component->name = name;
    component->mesh = std::move(mesh);
    return component;
}
} // namespace

TEST_CASE("ObjMeshIO::saveToFile()/loadFromFile() ComponentDatas round-trip")
{
    // 两个组件：alpha 1 个三角形，beta 2 个三角形
    ComponentDatas components;
    components.push_back(makeComponent("alpha",
        { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 } },
        { 0, 1, 2 },
        { 0, 3 }));
    components.push_back(makeComponent("beta",
        { { 0.0, 0.0, 1.0 }, { 1.0, 0.0, 1.0 }, { 0.0, 1.0, 1.0 }, { 1.0, 1.0, 1.0 } },
        { 0, 1, 2, 1, 3, 2 },
        { 0, 3, 6 }));

    const auto path = core::TempFile::instance().path().string() + "_comps.obj";
    {
        std::ofstream ofs(path);
        ObjMeshIO::saveToFile(components, ofs);
    }

    auto loaded = ObjMeshIO::loadFromFile(path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->size() == 2);

    // 组件名按 object 名还原
    const auto& alpha = (*loaded)[0];
    const auto& beta = (*loaded)[1];
    REQUIRE(alpha->name == "alpha");
    REQUIRE(beta->name == "beta");

    // 每个组件 MeshData 自包含：点数、面数与写出时一致
    REQUIRE(alpha->mesh->vertex_count_ == 3);
    REQUIRE(alpha->mesh->face_vertices_offset_.size() == 2);
    REQUIRE(beta->mesh->vertex_count_ == 4);
    REQUIRE(beta->mesh->face_vertices_offset_.size() == 3);

    // 面顶点为组件内局部点索引（不越界）
    for (Index idx : beta->mesh->face_vertices_) {
        REQUIRE(idx >= 0);
        REQUIRE(idx < beta->mesh->vertex_count_);
    }

    // 顶点坐标精确还原
    const auto& src_points = components[1]->mesh->vertex_positions_;
    for (size_t i = 0; i < src_points.size(); ++i) {
        for (int k = 0; k < 3; ++k) {
            REQUIRE(beta->mesh->vertex_positions_[i][k] == src_points[i][k]);
        }
    }
}

TEST_CASE("ObjMeshIO::saveToFile() skips components without mesh")
{
    // 无网格组件应被跳过，不产出 object
    ComponentDatas components;
    components.push_back(std::make_unique<ComponentData>());
    components.push_back(makeComponent("alpha",
        { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 } },
        { 0, 1, 2 },
        { 0, 3 }));

    const auto path = core::TempFile::instance().path().string() + "_skip.obj";
    {
        std::ofstream ofs(path);
        ObjMeshIO::saveToFile(components, ofs);
    }

    auto loaded = ObjMeshIO::loadFromFile(path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->size() == 1);
    REQUIRE((*loaded)[0]->name == "alpha");
}

TEST_CASE("ObjMeshIO fills group attributes by prefix in both import modes", "[obj]")
{
    const auto path = core::TempFile::instance().path().string() + "_face_groups.obj";
    {
        std::ofstream output(path, std::ios::binary);
        output << "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 0 0 1\nvn 0 0 1\n"
               << "o shell\ng patch_288\nf 1 3 4\n"
               << "g patch_7 zone12\nf -5//-1 -4//-1 -3//-1 -2//-1\n"
               << "g zone12 patch_7\nf 4 1 2\n"
               << "g # clear the active group\nf 2 3 5\ng default\nf 1 2 5\n"
               << "g patch_5 patch_6 zone13\nf 1 4 5\ng empty\n";
    }

    SECTION("关闭拆组时面组共用组件内顶点")
    {
        const auto loaded = ObjMeshIO::loadFromFile(path, false);
        REQUIRE(loaded);
        REQUIRE(loaded->size() == 1);
        CHECK((*loaded)[0]->name == "shell");

        const auto& mesh = *(*loaded)[0]->mesh;
        CHECK(mesh.vertex_count_ == 5);
        CHECK(mesh.face_vertices_offset_ == std::vector<Index> { 0, 3, 7, 10, 13, 16, 19 });
        CHECK(mesh.face_vertices_ == std::vector<Index> { 0, 1, 2, 0, 3, 1, 2, 2, 0, 3, 3, 1, 4, 0, 3, 4, 0, 2, 4 });
        // 同名属性只占一列，缺值或同名前缀冲突时填 0。
        CHECK(mesh.face_attributes_.size() == 2);
        CHECK(mesh.face_attributes_.at(patch_attribute_name) == std::vector<double> { 288.0, 7.0, 7.0, 0.0, 0.0, 0.0 });
        CHECK(mesh.face_attributes_.at("zone") == std::vector<double> { 0.0, 12.0, 12.0, 0.0, 0.0, 13.0 });
    }

    SECTION("开启拆组时沿用已有的组名划分")
    {
        const auto loaded = ObjMeshIO::loadFromFile(path);
        REQUIRE(loaded);
        REQUIRE(loaded->size() == 6);
        CHECK((*loaded)[0]->name == "patch_288");
        CHECK((*loaded)[1]->name == "patch_7 zone12");
        CHECK((*loaded)[2]->name == "zone12 patch_7");
        CHECK((*loaded)[3]->name == "Comp_3");
        CHECK((*loaded)[4]->name == "default");
        CHECK((*loaded)[5]->name == "patch_5 patch_6 zone13");
        CHECK((*loaded)[0]->mesh->face_attributes_.at(patch_attribute_name) == std::vector<double> { 288.0 });
        CHECK_FALSE((*loaded)[0]->mesh->face_attributes_.contains("zone"));
        for (size_t i : { 1, 2 }) {
            const auto& attributes = (*loaded)[i]->mesh->face_attributes_;
            CHECK(attributes.at(patch_attribute_name) == std::vector<double> { 7.0 });
            CHECK(attributes.at("zone") == std::vector<double> { 12.0 });
        }
        CHECK((*loaded)[3]->mesh->face_attributes_.empty());
        CHECK((*loaded)[4]->mesh->face_attributes_.empty());
        CHECK((*loaded)[5]->mesh->face_attributes_.at(patch_attribute_name) == std::vector<double> { 0.0 });
        CHECK((*loaded)[5]->mesh->face_attributes_.at("zone") == std::vector<double> { 13.0 });
    }
}

TEST_CASE("ObjMeshIO reads decimal group suffixes without rounding", "[obj]")
{
    const auto path = core::TempFile::instance().path().string() + "_patch_ids.obj";
    {
        std::ofstream output(path);
        output << "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
               << "g shell288\nf 1 2 3\ng patch_8a\nf 1 2 3\ng 42\nf 1 2 3\n"
               << "g patch_0\nf 1 2 3\ng patch_9007199254740992\nf 1 2 3\n"
               << "g patch_9007199254740993\nf 1 2 3\ng patch_18446744073709551616\nf 1 2 3\n";
    }
    const auto loaded = ObjMeshIO::loadFromFile(path, false);
    REQUIRE(loaded);
    REQUIRE(loaded->size() == 1);
    // 没有前缀或后缀的名称不产生属性，超出精度范围的编号不舍入保存。
    const auto& attributes = loaded->front()->mesh->face_attributes_;
    REQUIRE(attributes.size() == 2);
    CHECK(attributes.at(patch_attribute_name) == std::vector<double> { 0.0, 0.0, 0.0, 0.0, 9007199254740992.0, 0.0, 0.0 });
    CHECK(attributes.at("shell") == std::vector<double> { 288.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 });
}

TEST_CASE("ObjMeshIO preserves double coordinates and nearby distinct vertices", "[obj][precision]")
{
    const auto path = core::TempFile::instance().path().string() + "_double_precision.obj";
    {
        std::ofstream output(path);
        output << "v 1.00000001 0 0\nv 1.00000002 0 0\nv 1 1 0\n"
               << "v 170.6367188 -27.666666 33.2651329\n"
               << "g thin\nf 1 2 3\ng offset\nf 4 2 3\n";
    }
    const auto loaded = ObjMeshIO::loadFromFile(path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->size() == 2);
    const auto merged = ObjMeshIO::loadFromFile(path, false);
    REQUIRE(merged);
    REQUIRE(merged->size() == 1);
    CHECK((*merged)[0]->name == "Comp_0");
    CHECK((*merged)[0]->mesh->vertex_count_ == 4);
    CHECK((*merged)[0]->mesh->face_attributes_.empty());
    const auto& thin = *(*loaded)[0]->mesh;
    REQUIRE(thin.vertex_positions_.size() == 3);
    CHECK(thin.vertex_positions_[0][0] == 1.00000001);
    CHECK(thin.vertex_positions_[1][0] == 1.00000002);
    CHECK(thin.vertex_positions_[0] != thin.vertex_positions_[1]);
    // 近邻点不能因 float 舍入变为重合点，使原来非零面积的三角形退化。
    CHECK(thin.vertex_positions_[1][0] - thin.vertex_positions_[0][0] > 0);
    const auto& offset = *(*loaded)[1]->mesh;
    const std::array<double, 3> expected_offset { 170.6367188, -27.666666, 33.2651329 };
    // tinyobj 的十进制累加存在 double 舍入；4 ULP 仍远小于 float32 的误差。
    for (int axis = 0; axis < 3; ++axis)
        CHECK_THAT(offset.vertex_positions_[0][axis], Catch::Matchers::WithinULP(expected_offset[axis], 4));

    const auto round_trip_path = core::TempFile::instance().path().string() + "_double_roundtrip.obj";
    {
        std::ofstream output(round_trip_path);
        ObjMeshIO::saveToFile(*loaded, output);
    }
    const auto restored = ObjMeshIO::loadFromFile(round_trip_path);
    REQUIRE(restored.has_value());
    REQUIRE(restored->size() == loaded->size());
    for (std::size_t i = 0; i < loaded->size(); ++i) {
        CHECK((*restored)[i]->name == (*loaded)[i]->name);
        const auto& expected = (*loaded)[i]->mesh->vertex_positions_;
        const auto& actual = (*restored)[i]->mesh->vertex_positions_;
        REQUIRE(actual.size() == expected.size());
        for (std::size_t vertex = 0; vertex < expected.size(); ++vertex)
            for (int axis = 0; axis < 3; ++axis)
                CHECK_THAT(actual[vertex][axis], Catch::Matchers::WithinULP(expected[vertex][axis], 4));
        CHECK((*restored)[i]->mesh->face_vertices_ == (*loaded)[i]->mesh->face_vertices_);
    }
}

TEST_CASE("ObjMeshIO keeps repeated declarations of the active group in one component")
{
    const auto path = core::TempFile::instance().path().string() + "_repeated_group.obj";
    constexpr std::array records {
        "g patch_1 wall", "v 0 0 0", "v 1 0 0", "v 1 1 0", "v 0 1 0", "v 2 0 0",
        "f 1 2 3 4", "g\tpatch_1  wall  # same group names"
    };
    for (const char* line_ending : { "\n", "\r\n", "\r" }) {
        CAPTURE(line_ending);
        {
            std::ofstream output(path, std::ios::binary);
            output << "\xef\xbb\xbf";
            for (const char* line : records) {
                output << line << line_ending;
            }
            output << "f 2 5 3";
        }

        const auto loaded = ObjMeshIO::loadFromFile(path);
        REQUIRE(loaded.has_value());
        REQUIRE(loaded->size() == 1);
        const auto& component = *loaded->front();
        CHECK(component.name == "patch_1 wall");

        const auto& mesh = *component.mesh;
        CHECK(mesh.vertex_count_ == 5);
        CHECK(mesh.face_vertices_offset_ == std::vector<Index> { 0, 4, 7 });
        CHECK(mesh.face_vertices_ == std::vector<Index> { 0, 1, 2, 3, 1, 4, 2 });
    }
}

TEST_CASE("ObjMeshIO merges recurring groups within one object")
{
    const auto path = core::TempFile::instance().path().string() + "_recurring_groups.obj";
    {
        std::ofstream output(path);
        output << "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 2 0 0\n"
               << "o object\n"
               << "g patch_1\nf 1 2 3 4\n"
               << "g patch_2\nf 1 3 5\n"
               << "g patch_1\nf 2 5 3\n";
    }

    const auto loaded = ObjMeshIO::loadFromFile(path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->size() == 2);
    CHECK((*loaded)[0]->name == "patch_1");
    CHECK((*loaded)[1]->name == "patch_2");

    const auto& patch = *(*loaded)[0]->mesh;
    CHECK(patch.vertex_count_ == 5);
    CHECK(patch.face_vertices_offset_ == std::vector<Index> { 0, 4, 7 });
    CHECK(patch.face_vertices_ == std::vector<Index> { 0, 1, 2, 3, 1, 4, 2 });
    CHECK(patch.face_attributes_.at(patch_attribute_name) == std::vector<double> { 1.0, 1.0 });
    CHECK((*loaded)[1]->mesh->face_vertices_offset_ == std::vector<Index> { 0, 3 });
    CHECK((*loaded)[1]->mesh->face_attributes_.at(patch_attribute_name) == std::vector<double> { 2.0 });
}

TEST_CASE("ObjMeshIO keeps ungrouped faces separate from recurring named groups")
{
    const auto path = core::TempFile::instance().path().string() + "_empty_group.obj";
    {
        std::ofstream output(path);
        output << "v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\n"
               << "g patch_1\nf 1 2 3\n"
               << "g # clear the active group\nf 1 2 4\n"
               << "g patch_1\nf 2 4 3\n";
    }

    const auto loaded = ObjMeshIO::loadFromFile(path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->size() == 2);
    CHECK((*loaded)[0]->name == "patch_1");
    CHECK((*loaded)[0]->mesh->face_vertices_offset_ == std::vector<Index> { 0, 3, 6 });
    CHECK((*loaded)[0]->mesh->face_vertices_ == std::vector<Index> { 0, 1, 2, 1, 3, 2 });
    CHECK((*loaded)[1]->name != "patch_1");
    CHECK((*loaded)[1]->mesh->face_vertices_offset_ == std::vector<Index> { 0, 3 });
}

TEST_CASE("ObjMeshIO keeps matching object and group names in separate components")
{
    const auto path = core::TempFile::instance().path().string() + "_object_boundaries.obj";
    {
        std::ofstream output(path);
        output << "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
               << "o patch_1\nf 1 2 3\n"
               << "g patch_1\nf 1 2 3\n"
               << "o patch_1\nf 1 2 3\n"
               << "g patch_1\nf 1 2 3\n";
    }

    const auto loaded = ObjMeshIO::loadFromFile(path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->size() == 4);
    for (const auto& component : *loaded) {
        CHECK(component->name == "patch_1");
        CHECK(component->mesh->face_vertices_offset_ == std::vector<Index> { 0, 3 });
    }

    // 同名对象仍分别导入，只有显式声明 g 的面获得组号。
    const auto merged = ObjMeshIO::loadFromFile(path, false);
    REQUIRE(merged);
    REQUIRE(merged->size() == 2);
    for (const auto& component : *merged) {
        CHECK(component->name == "patch_1");
        CHECK(component->mesh->face_vertices_offset_ == std::vector<Index> { 0, 3, 6 });
        const auto& group_ids = component->mesh->face_attributes_.at(patch_attribute_name);
        REQUIRE(group_ids.size() == 2);
        CHECK(group_ids.front() == 0.0);
        CHECK(group_ids.back() == 1.0);
    }
}
