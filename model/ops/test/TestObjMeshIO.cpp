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
