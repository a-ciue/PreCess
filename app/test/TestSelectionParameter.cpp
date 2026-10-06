/**
 * @file TestSelectionParameter.cpp
 * @brief 选择器空值从 QML 适配层写入功能参数的回归测试
 */
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
