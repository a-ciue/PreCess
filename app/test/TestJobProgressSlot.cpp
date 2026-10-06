/**
 * @file TestJobProgressSlot.cpp
 * @brief JobProgressSlot 单元测试：last-wins 合并、value/label 成对、终态门闩、并发写读
 */
#include "JobProgressSlot.h"

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <thread>

TEST_CASE("Progress slot: poll without write returns false", "[JobProgressSlot]")
{
    JobProgressSlot slot;
    double value = -1.0;
    std::string label = "untouched";
    REQUIRE_FALSE(slot.poll(value, label));
    REQUIRE(label == "untouched"); // 无新值不改出参
}

TEST_CASE("Progress slot: last-wins coalescing across N writes", "[JobProgressSlot]")
{
    JobProgressSlot slot;
    for (int i = 1; i <= 100; ++i)
        slot.write(i / 100.0, "step-" + std::to_string(i));

    double value = 0.0;
    std::string label;
    REQUIRE(slot.poll(value, label)); // 100 写 1 轮询：只见最新
    REQUIRE(value == 1.0);
    REQUIRE(label == "step-100");
    REQUIRE_FALSE(slot.poll(value, label)); // 已消费：无新值
}

TEST_CASE("Progress slot: value and label stay paired", "[JobProgressSlot]")
{
    JobProgressSlot slot;
    double value = 0.0;
    std::string label;
    for (int i = 1; i <= 50; ++i) {
        slot.write(i / 50.0, "pair-" + std::to_string(i));
        REQUIRE(slot.poll(value, label));
        REQUIRE(value == i / 50.0); // 每次轮询拿到的是同一次写入的 value+label
        REQUIRE(label == "pair-" + std::to_string(i));
    }
}

TEST_CASE("Progress slot: close gates poll and drops late writes", "[JobProgressSlot]")
{
    JobProgressSlot slot;
    slot.write(0.5, "before-close");
    slot.close();

    double value = 0.0;
    std::string label;
    REQUIRE_FALSE(slot.poll(value, label)); // close 清 pending：终态前未消费的进度不再下发

    slot.write(0.9, "late"); // 迟到写入（终态后）作废
    REQUIRE_FALSE(slot.poll(value, label));
}

TEST_CASE("Progress slot: reset clears residue and reopens after close", "[JobProgressSlot]")
{
    JobProgressSlot slot;
    slot.write(0.5, "old-job");
    slot.close();
    slot.reset(); // 新任务开始

    double value = 0.0;
    std::string label;
    REQUIRE_FALSE(slot.poll(value, label)); // 上一任务残留不带入
    slot.write(0.25, "new-job");
    REQUIRE(slot.poll(value, label)); // 门闩已解除
    REQUIRE(value == 0.25);
    REQUIRE(label == "new-job");
}

TEST_CASE("Progress slot: concurrent writes and polls stay consistent", "[JobProgressSlot]")
{
    JobProgressSlot slot;
    constexpr int kWrites = 1000;
    std::thread writer([&] {
        for (int i = 1; i <= kWrites; ++i)
            slot.write(i / static_cast<double>(kWrites), "w-" + std::to_string(i));
    });

    // 轮询侧持续消费（value/label 必须来自同一次写入）
    int polls = 0;
    double value = 0.0;
    std::string label;
    while (slot.poll(value, label)) {
        const std::string expected = "w-" + std::to_string(static_cast<int>(value * kWrites + 0.5));
        REQUIRE(label == expected);
        ++polls;
    }
    writer.join();

    REQUIRE(slot.poll(value, label)); // 末次写入必可见
    REQUIRE(value == 1.0);
    REQUIRE(label == "w-" + std::to_string(kWrites));
    REQUIRE(polls <= kWrites); // 合并：轮询次数不超过写入次数
}
TEST_CASE("Progress-only updates preserve the latest nonempty status text", "[JobProgressSlot]")
{
    JobProgressSlot slot;
    slot.write(0.1, "computing");
    slot.write(0.5, "");
    double value;
    std::string label;
    REQUIRE(slot.poll(value, label));
    REQUIRE(value == 0.5);
    REQUIRE(label == "computing");
    slot.write(1.0, "installed");
    REQUIRE(slot.poll(value, label));
    REQUIRE(label == "installed");
    slot.reset();
    slot.write(0.2, "");
    REQUIRE(slot.poll(value, label));
    REQUIRE(label.empty());
}
