#include "MeshIDMap.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("MeshIDMap basic insert/getLocal")
{
    MeshIDMap map;

    auto g0 = map.insert(10, 0);
    auto g1 = map.insert(10, 1);
    auto g2 = map.insert(10, 2);

    REQUIRE(g0 == 0);
    REQUIRE(g1 == 1);
    REQUIRE(g2 == 2);

    auto p1 = map.getLocal(g1);
    REQUIRE(p1.first == 10);
    REQUIRE(p1.second == 1);

    REQUIRE(map.size() == 3);
    REQUIRE(map.freeSize() == 0);
}

TEST_CASE("MeshIDMap remove marks invalid and adds to free pool")
{
    MeshIDMap map;

    auto g0 = map.insert(10, 0);
    auto g1 = map.insert(10, 1);

    REQUIRE(map.remove(g0) == true);

    auto p0 = map.getLocal(g0);
    REQUIRE(p0.first == MeshIDMap::kInvalidComponent);
    REQUIRE(p0.second == MeshIDMap::kInvalidLocal);

    REQUIRE(map.freeSize() == 1);

    // 重复 remove 不应增加 free pool
    REQUIRE(map.remove(g0) == false);
    REQUIRE(map.freeSize() == 1);
}

TEST_CASE("MeshIDMap insert reuses freed ids (unordered, any one)")
{
    MeshIDMap map;

    auto g0 = map.insert(10, 0);
    auto g1 = map.insert(10, 1);
    auto g2 = map.insert(10, 2);

    REQUIRE(map.remove(g1) == true);
    REQUIRE(map.remove(g2) == true);
    REQUIRE(map.freeSize() == 2);

    // 新插入会复用 g1 或 g2（unordered_set 无序）
    auto g3 = map.insert(20, 99);

    REQUIRE((g3 == g1 || g3 == g2));
    auto p3 = map.getLocal(g3);
    REQUIRE(p3.first == 20);
    REQUIRE(p3.second == 99);

    // free pool 应该少 1
    REQUIRE(map.freeSize() == 1);
}

TEST_CASE("MeshIDMap insertRange works")
{
    MeshIDMap map;

    std::vector<MeshIDMap::GlobalID> gids;
    map.insertRange(7, 0, 5, gids);

    REQUIRE(gids.size() == 5);
    // 因为初始为空，应该是 0..4
    REQUIRE(gids[0] == 0);
    REQUIRE(gids[4] == 4);

    for (int i = 0; i < 5; ++i) {
        auto p = map.getLocal(gids[i]);
        REQUIRE(p.first == 7);
        REQUIRE(p.second == i);
    }
}