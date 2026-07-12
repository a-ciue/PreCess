/**
 * @file TestTetGenLibHandler.cpp
 * @brief TetGenLibHandler 单元测试
 */
#include "TetGenLibHandler.h"

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

using namespace systems::algo;
using namespace core;

TEST_CASE("TetGenLibHandler::args_type() returns correct parameter count and types")
{
    TetGenLibHandler handler;
    auto args = handler.args_type();

    REQUIRE(args.size() == 7);

    SECTION("Parameter 0: Combo - 是否仅使用最大表面壳")
    {
        CHECK(args[0].type == ArgTypeEnum::Combo);
        CHECK(args[0].name == "是否仅使用最大表面壳");
        CHECK(args[0].content == "是,否");
    }

    SECTION("Parameter 1: Float - 质量参数 q")
    {
        CHECK(args[1].type == ArgTypeEnum::Float);
        CHECK(args[1].name == "质量参数 q（0表示关闭）");
        CHECK(args[1].content == "1.2");
    }

    SECTION("Parameter 2: Float - 最大单元体积 a")
    {
        CHECK(args[2].type == ArgTypeEnum::Float);
        CHECK(args[2].name == "最大单元体积 a（0表示关闭）");
        CHECK(args[2].content == "0");
    }

    SECTION("Parameter 3: Combo - 是否保留原始表面")
    {
        CHECK(args[3].type == ArgTypeEnum::Combo);
        CHECK(args[3].name == "是否保留原始表面");
        CHECK(args[3].content == "是,否");
    }

    SECTION("Parameter 4: Combo - 是否仅检测自交")
    {
        CHECK(args[4].type == ArgTypeEnum::Combo);
        CHECK(args[4].name == "是否仅检测自交");
        CHECK(args[4].content == "是,否|1");
    }

    SECTION("Parameter 5: Combo - 输出方式")
    {
        CHECK(args[5].type == ArgTypeEnum::Combo);
        CHECK(args[5].name == "输出方式");
        CHECK(args[5].content == "新建模型,替换当前模型");
        CHECK(args[5].desc.find("默认新建模型") != std::string::npos);
    }

    SECTION("Parameter 6: Text - 高级 TetGen 参数")
    {
        CHECK(args[6].type == ArgTypeEnum::Text);
        CHECK(args[6].name == "高级 TetGen 参数");
        CHECK(args[6].content == "");
        CHECK(args[6].desc.find("仅靠 switches") != std::string::npos);
    }

}
