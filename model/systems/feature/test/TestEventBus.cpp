#include "EventBus.h"

#include <catch2/catch_test_macros.hpp>

using core::EventBus;

namespace {
struct TestEvent {
    int value { 0 };
};
struct OtherEvent { };
}

TEST_CASE("EventBus subscribe and publish", "[EventBus]")
{
    EventBus bus;
    int received = 0;
    auto sub = bus.subscribe<TestEvent>([&](const TestEvent& e) { received += e.value; });
    bus.publish(TestEvent { 1 });
    bus.publish(TestEvent { 2 });
    REQUIRE(received == 3);
}

TEST_CASE("EventBus multiple subscribers", "[EventBus]")
{
    EventBus bus;
    int a = 0, b = 0;
    auto sub_a = bus.subscribe<TestEvent>([&](const TestEvent& e) { a += e.value; });
    auto sub_b = bus.subscribe<TestEvent>([&](const TestEvent& e) { b += e.value * 2; });
    bus.publish(TestEvent { 3 });
    REQUIRE(a == 3);
    REQUIRE(b == 6);
}

TEST_CASE("EventBus only dispatches matching event type", "[EventBus]")
{
    EventBus bus;
    bool called = false;
    auto sub = bus.subscribe<TestEvent>([&](const TestEvent&) { called = true; });
    bus.publish(OtherEvent {});
    REQUIRE_FALSE(called);
}

TEST_CASE("EventBus subscription RAII unsubscribes on destruction", "[EventBus]")
{
    EventBus bus;
    int count = 0;
    {
        auto sub = bus.subscribe<TestEvent>([&](const TestEvent&) { ++count; });
        bus.publish(TestEvent {});
        REQUIRE(count == 1);
    }
    bus.publish(TestEvent {});
    REQUIRE(count == 1);
}

TEST_CASE("EventBus Subscription::reset unsubscribes", "[EventBus]")
{
    EventBus bus;
    int count = 0;
    auto sub = bus.subscribe<TestEvent>([&](const TestEvent&) { ++count; });
    REQUIRE(sub);
    sub.reset();
    REQUIRE_FALSE(sub);
    bus.publish(TestEvent {});
    REQUIRE(count == 0);
}

TEST_CASE("EventBus move semantics transfer subscription", "[EventBus]")
{
    EventBus bus;
    int count = 0;
    auto sub1 = bus.subscribe<TestEvent>([&](const TestEvent&) { ++count; });
    EventBus::Subscription sub2 = std::move(sub1);
    REQUIRE_FALSE(sub1);
    REQUIRE(sub2);
    bus.publish(TestEvent {});
    REQUIRE(count == 1);

    sub1 = std::move(sub2);
    REQUIRE(sub1);
    REQUIRE_FALSE(sub2);
    bus.publish(TestEvent {});
    REQUIRE(count == 2);
}

TEST_CASE("EventBus unsubscribing inside handler is safe (snapshot semantics)", "[EventBus]")
{
    EventBus bus;
    int count_a = 0, count_b = 0;
    EventBus::Subscription sub_b;
    auto sub_a = bus.subscribe<TestEvent>([&](const TestEvent&) {
        ++count_a;
        sub_b.reset(); // 回调内退订 B
    });
    sub_b = bus.subscribe<TestEvent>([&](const TestEvent&) { ++count_b; });

    bus.publish(TestEvent {}); // 快照语义：B 本次仍被调用
    REQUIRE(count_a == 1);
    REQUIRE(count_b == 1);

    bus.publish(TestEvent {}); // B 已退订，不再收到
    REQUIRE(count_a == 2);
    REQUIRE(count_b == 1);
}
