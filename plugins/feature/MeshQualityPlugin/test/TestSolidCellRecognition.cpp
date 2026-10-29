#include "SolidCellRecognition.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <vtkCellType.h>

#include <algorithm>
#include <array>
#include <vector>

using namespace systems::feature;

TEST_CASE("Solid recognition matches collapsed hex face topology", "[MeshQualityPlugin]")
{
    const int type = GENERATE(VTK_TETRA, VTK_PYRAMID, VTK_WEDGE);
    std::vector<Index> ids;
    if (type == VTK_TETRA)
        ids = { 10, 20, 30, 30, 40, 40, 40, 40 };
    if (type == VTK_PYRAMID)
        ids = { 10, 20, 30, 40, 50, 50, 50, 50 };
    if (type == VTK_WEDGE)
        ids = { 10, 20, 20, 30, 40, 50, 50, 60 };
    const auto original = ids;
    const auto record = recognizeSolidCell(17, VTK_HEXAHEDRON, ids);
    CHECK(record.source_cell_id == 17);
    CHECK(record.source_type == VTK_HEXAHEDRON);
    CHECK(record.evaluated_type == type);
    CHECK(record.recognition == SolidCellRecognition::Collapsed);
    CHECK(record.point_ids.size() == static_cast<std::size_t>(solidPointCount(type)));
    CHECK(ids == original);
    // 排序后的全局/局部节点编号不决定朝向；反向编号也必须得到同一拓扑结论。
    for (auto& id : ids)
        id = 100 - id;
    CHECK(recognizeSolidCell(17, VTK_HEXAHEDRON, ids).evaluated_type == type);
}

TEST_CASE("Solid recognition rejects equal node counts with incompatible faces", "[MeshQualityPlugin]")
{
    const auto ids = GENERATE(
        std::vector<Index> { 0, 1, 2, 3, 4, 5, 4, 5 },
        std::vector<Index> { 0, 1, 0, 2, 3, 4, 5, 3 },
        std::vector<Index> { 0, 1, 0, 2, 3, 4, 3, 4 },
        std::vector<Index> { 0, 1, 0, 1, 2, 3, 2, 3 });
    const auto record = recognizeSolidCell(2, VTK_HEXAHEDRON, ids);
    CHECK(record.evaluated_type == VTK_EMPTY_CELL);
    CHECK(record.recognition == SolidCellRecognition::Unrecognized);
    CHECK(record.point_ids.empty());
}

TEST_CASE("Solid recognition handles collapsed standard wedge and pyramid sources", "[MeshQualityPlugin]")
{
    const std::vector<Index> hex_pyramid { 0, 1, 2, 3, 0, 1, 4, 4 };
    CHECK(recognizeSolidCell(0, VTK_HEXAHEDRON, hex_pyramid).evaluated_type == VTK_PYRAMID);
    const std::vector<Index> wedge_pyramid { 0, 1, 2, 0, 3, 4 };
    CHECK(recognizeSolidCell(1, VTK_WEDGE, wedge_pyramid).evaluated_type == VTK_PYRAMID);
    const std::vector<Index> pyramid_tet { 0, 0, 1, 2, 3 };
    CHECK(recognizeSolidCell(2, VTK_PYRAMID, pyramid_tet).evaluated_type == VTK_TETRA);
}

TEST_CASE("Solid recognition covers every orientation preserving hex rotation", "[MeshQualityPlugin]")
{
    // 由立方体坐标独立生成 24 种保向旋转，使折叠边/面的方向不依赖某一组固定槽位。
    constexpr std::array<std::array<int, 3>, 8> corners { {
        { -1, -1, -1 },
        { 1, -1, -1 },
        { 1, 1, -1 },
        { -1, 1, -1 },
        { -1, -1, 1 },
        { 1, -1, 1 },
        { 1, 1, 1 },
        { -1, 1, 1 },
    } };
    const int type = GENERATE(VTK_TETRA, VTK_PYRAMID, VTK_WEDGE);
    const std::vector<Index> source = type == VTK_TETRA ? std::vector<Index> { 0, 1, 2, 2, 3, 3, 3, 3 }
        : type == VTK_PYRAMID                           ? std::vector<Index> { 0, 1, 2, 3, 4, 4, 4, 4 }
                                                        : std::vector<Index> { 0, 1, 1, 2, 3, 4, 4, 5 };
    std::array<int, 3> axes { 0, 1, 2 };
    int tested = 0;
    do {
        const int inversions = (axes[0] > axes[1]) + (axes[0] > axes[2]) + (axes[1] > axes[2]);
        for (int signs = 0; signs < 8; ++signs) {
            std::array<int, 3> factors;
            for (int i = 0; i < 3; ++i)
                factors[i] = signs & (1 << i) ? -1 : 1;
            if (factors[0] * factors[1] * factors[2] * (inversions % 2 ? -1 : 1) != 1)
                continue;
            std::vector<Index> rotated(8);
            for (std::size_t i = 0; i < corners.size(); ++i) {
                const std::array<int, 3> corner { corners[i][axes[0]] * factors[0], corners[i][axes[1]] * factors[1], corners[i][axes[2]] * factors[2] };
                const auto j = std::find(corners.begin(), corners.end(), corner) - corners.begin();
                REQUIRE(j < 8);
                rotated[j] = source[i];
            }
            const auto record = recognizeSolidCell(0, VTK_HEXAHEDRON, rotated);
            CHECK(record.evaluated_type == type);
            CHECK(record.recognition == SolidCellRecognition::Collapsed);
            ++tested;
        }
    } while (std::next_permutation(axes.begin(), axes.end()));
    CHECK(tested == 24);
}

TEST_CASE("Solid recognition keeps standard unknown and malformed cells distinct", "[MeshQualityPlugin]")
{
    SECTION("standard wedge")
    {
        const std::vector<Index> ids { 4, 8, 3, 7, 2, 1 };
        const auto record = recognizeSolidCell(9, VTK_WEDGE, ids);
        CHECK(record.recognition == SolidCellRecognition::Original);
        CHECK(record.evaluated_type == VTK_WEDGE);
        CHECK(record.point_ids == ids);
    }
    SECTION("unknown original cell type")
    {
        const std::vector<Index> ids { 0, 1, 2, 3, 4, 5 };
        CHECK(recognizeSolidCell(0, VTK_POLYHEDRON, ids).recognition == SolidCellRecognition::Unsupported);
    }
    SECTION("wrong node count")
    {
        const std::vector<Index> ids { 0, 1, 2, 3, 4, 5 };
        CHECK(recognizeSolidCell(0, VTK_HEXAHEDRON, ids).recognition == SolidCellRecognition::Unrecognized);
    }
}
