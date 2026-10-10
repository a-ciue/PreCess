/**
 * @file TestSelectionParameter.cpp
 * @brief 选择器空值从 QML 适配层写入功能参数的回归测试
 */
#include "FeatureEvents.h"
#include "FeatureHandler.h"
#include "FeatureRegistrar.h"
#include "FeatureSystem.h"
#include "ModelLayer.h"
#include "QFeatureSystemAdaptor.h"
#include "QSelection.h"

#include <QJSValue>
#include <catch2/catch_test_macros.hpp>

namespace {
class SelectionFeature : public systems::feature::FeatureHandler {
public:
    void setup(systems::feature::FeatureRegistrar& reg, systems::feature::FeatureContext&) override
    {
        reg.addParameter({ ArgTypeEnum::Selector, "目标", "Face", "" });
    }
};
class SnapshotFeature : public systems::feature::FeatureHandler {
public:
    void setup(systems::feature::FeatureRegistrar& reg, systems::feature::FeatureContext&) override
    {
        reg.addParameter({ ArgTypeEnum::Int, "次数", "4", "" });
        reg.addParameter({ ArgTypeEnum::Float, "尺寸", "1.5", "" });
        reg.addParameter({ ArgTypeEnum::Text, "文字", "初始值", "" });
        reg.addParameter({ ArgTypeEnum::Bool, "开关", "true", "" });
        reg.addParameter({ ArgTypeEnum::Path, "路径", "input.obj", "" });
        reg.addParameter({ ArgTypeEnum::Combo, "模式", "甲,乙|1", "" });
        reg.addParameter({ ArgTypeEnum::Button, "执行", "", "" });
    }
};
}

TEST_CASE("Selection parameter requires typed selections and preserves values on invalid input")
{
    core::EventBus bus;
    ModelLayer model_layer;
    systems::feature::FeatureSystem system(model_layer, bus);
    systems::feature::HandlerMetaData meta;
    meta.name = "SelectionTest";
    systems::feature::FeatureSystem::SystemHandlerPtr handler { std::make_unique<SelectionFeature>().release() };
    REQUIRE(system.registerHandler(meta, std::move(handler)));
    systems::feature::QFeatureSystemAdaptor adaptor(system);
    QSelection selection(std::make_unique<Selection>());
    selection.get()->type = ElementEnum::Face;
    selection.get()->ids = { 7 };
    REQUIRE(adaptor.setParameter("SelectionTest", 0, QVariant::fromValue(&selection)));
    const auto* params = system.params("SelectionTest");
    REQUIRE(params != nullptr);
    REQUIRE(*params->value(0).get<ArgTypeEnum::Selector>() == selection.get());

    SECTION("Empty selection object clears the stored selection")
    {
        QSelection empty_selection;
        REQUIRE(adaptor.setParameter("SelectionTest", 0, QVariant::fromValue(&empty_selection)));
        const auto* value = params->value(0).get<ArgTypeEnum::Selector>();
        REQUIRE(value != nullptr);
        CHECK_FALSE(*value);
    }
    SECTION("Typed null selection pointer clears the stored selection")
    {
        REQUIRE(adaptor.setParameter("SelectionTest", 0, QVariant::fromValue(static_cast<QSelection*>(nullptr))));
        CHECK_FALSE(*params->value(0).get<ArgTypeEnum::Selector>());
    }
    SECTION("Untyped null and mismatched empty values preserve the stored selection")
    {
        const QVariant invalid_values[] = {
            QJSValue(QJSValue::NullValue).toVariant(),
            QJSValue(QJSValue::UndefinedValue).toVariant(),
            QVariant {},
            QVariant::fromValue(static_cast<QObject*>(nullptr)),
            QVariant::fromValue(static_cast<void*>(nullptr)),
            QVariant(QString {}),
        };
        for (const auto& invalid_value : invalid_values) {
            CHECK_FALSE(adaptor.setParameter("SelectionTest", 0, invalid_value));
            CHECK(*params->value(0).get<ArgTypeEnum::Selector>() == selection.get());
        }
    }
    SECTION("Wrong non-null type preserves the stored selection")
    {
        CHECK_FALSE(adaptor.setParameter("SelectionTest", 0, QStringLiteral("invalid")));
        CHECK(*params->value(0).get<ArgTypeEnum::Selector>() == selection.get());
    }
    system.unregisterHandler(meta);
}

TEST_CASE("Feature parameter snapshots preserve typed values without emitting events", "[parameters][Qt]")
{
    core::EventBus bus;
    ModelLayer model_layer;
    systems::feature::FeatureSystem system(model_layer, bus);
    systems::feature::HandlerMetaData meta;
    meta.name = "SnapshotTest";
    REQUIRE(system.registerHandler(meta, systems::feature::FeatureSystem::SystemHandlerPtr { std::make_unique<SnapshotFeature>().release() }));
    systems::feature::QFeatureSystemAdaptor adaptor(system);
    int notifications = 0;
    auto subscription = bus.subscribe<systems::feature::ParameterChangedEvent>([&](const auto&) { ++notifications; });
    auto values = adaptor.getParameterValues("SnapshotTest");
    REQUIRE(values.size() == 7);
    CHECK(values[0].toLongLong() == 4);
    CHECK(values[1].toDouble() == 1.5);
    CHECK(values[2].toString() == QStringLiteral("初始值"));
    CHECK(values[3].toBool());
    CHECK(values[4].toString() == QStringLiteral("input.obj"));
    CHECK(values[5].toInt() == 1);
    CHECK(values[6].toInt() == 0);
    CHECK(notifications == 0);
    REQUIRE(adaptor.setParameter("SnapshotTest", 0, 42));
    REQUIRE(adaptor.setParameter("SnapshotTest", 3, false));
    REQUIRE(adaptor.setParameter("SnapshotTest", 4, QStringLiteral("模型.obj")));
    REQUIRE(adaptor.setParameter("SnapshotTest", 5, 0));
    values = adaptor.getParameterValues("SnapshotTest");
    CHECK(values[0].toLongLong() == 42);
    CHECK_FALSE(values[3].toBool());
    CHECK(values[4].toString() == QStringLiteral("模型.obj"));
    CHECK(values[5].toInt() == 0);
    CHECK(notifications == 4);
    CHECK(adaptor.getParameterValues("missing").isEmpty());
    system.unregisterHandler(meta);
}
