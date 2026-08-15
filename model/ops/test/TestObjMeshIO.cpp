/**
 * @file TestObjMeshIO.cpp
 * @brief ObjMeshIO 组件级读写（ComponentDatas）回环测试
 */
#include "ObjMeshIO.h"
#include "ComponentData.h"
#include "MeshData.h"
#include "TempFile.h"

#include <catch2/catch_test_macros.hpp>
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
