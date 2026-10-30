/**
 * @file TestScalarRangeState.cpp
 * @brief 在无窗口 QML 引擎中验证范围偏好、联动和执行身份规则。
 */
#include <QCoreApplication>
#include <QJSValue>
#include <QQmlComponent>
#include <QQmlEngine>
#include <catch2/catch_test_macros.hpp>
#include <memory>

namespace {
/** @brief 仅创建 Core 应用和纯 QtObject，不初始化图形或渲染资源。 */
QCoreApplication& application()
{
    static int argc = 1;
    static char name[] = "TestScalarRangeState";
    static char* argv[] = { name, nullptr };
    static QCoreApplication app(argc, argv);
    return app;
}

/** @brief 加载生产状态对象，通过 JavaScript 调用保持 QML 实际参数类型。 */
struct RangeFixture {
    QQmlEngine engine;
    std::unique_ptr<QObject> object;

    RangeFixture()
    {
        QQmlComponent component(&engine, QUrl("qrc:/scalar-state/ScalarRangeState.qml"));
        INFO(component.errorString().toStdString());
        REQUIRE(component.isReady());
        object.reset(component.create());
        REQUIRE(object);
        engine.globalObject().setProperty("ranges", engine.newQObject(object.get()));
    }

    /** @brief 执行一个状态场景并把脚本异常作为明确测试失败呈现。 */
    QJSValue run(const char* script)
    {
        auto value = engine.evaluate(QString::fromUtf8(script));
        INFO(value.toString().toStdString());
        REQUIRE_FALSE(value.isError());
        return value;
    }
};
}

TEST_CASE("Scalar UI ranges preserve independent preferences and linked reset rules", "[scalar-range][qml-state]")
{
    application();
    RangeFixture fixture;
    CHECK(fixture.run(R"JS(
        var key = ranges.ensure(7, 'value', 2, {valid:true, minimum:0.2, maximum:0.8, rangeAvailable:true});
        ranges.editRange(key, false, -1, 2);
        ranges.editRange(key, true, 0.3, 0.6);
        ranges.resetRange(key, true);
        var state = ranges.parameters(key);
        state.mappingLower === -1 && state.mappingUpper === 2
            && state.displayLower === 0.2 && state.displayUpper === 0.8 && !state.filtered;
    )JS")
            .toBool());
    CHECK(fixture.run(R"JS(
        ranges.setLinked(key, false, true);
        var state = ranges.parameters(key);
        state.linked && state.filtered && state.displayLower === -1 && state.displayUpper === 2;
    )JS")
            .toBool());
    CHECK(fixture.run(R"JS(
        ranges.resetRange(key, false);
        ranges.editRange(key, false, 0.2, 0.8);
        var state = ranges.parameters(key);
        state.linked && !state.filtered && ranges.configuration(key).display === null;
    )JS")
            .toBool());
    CHECK(fixture.run(R"JS(
        ranges.editRange(key, true, 0.4, 0.5);
        ranges.setLinked(key, false, false);
        ranges.editRange(key, false, -2, 3);
        var state = ranges.parameters(key);
        state.mappingLower === -2 && state.displayLower === 0.4 && state.displayUpper === 0.5
            && state.filtered && !state.linked
            && !ranges.editRange(key, false, 2, 1) && !ranges.editRange(key, true, 0, Infinity);
    )JS")
            .toBool());
}

TEST_CASE("Scalar UI ranges keep property identity and publish only active configuration", "[scalar-range][qml-state]")
{
    application();
    RangeFixture fixture;
    CHECK(fixture.run(R"JS(
        var requests = [];
        ranges.applyRequested.connect(function(component, attribute, type, configuration) {
            requests.push({component:component, attribute:attribute, type:type, configuration:configuration});
        });
        var a = ranges.ensure(7, 'value', 2, {valid:true, minimum:0.25, maximum:0.75, rangeAvailable:true});
        var b = ranges.ensure(7, 'vector', 2, {valid:false});
        ranges.editRange(a, false, 0.3, 0.6);
        ranges.editRange(b, false, 100, 200);
        requests.length === 0 && ranges.configuration(b).mapping[0] === 100
            && ranges.configuration(a).mapping[0] === 0.3 && !ranges.setLinked(b, false, true);
    )JS")
            .toBool());
    CHECK(fixture.run(R"JS(
        ranges.apply(a);
        ranges.setActive(7, 'value', 2);
        ranges.setLinked(a, false, true);
        var afterLink = requests.length;
        ranges.editRange(a, false, 0.35, 0.55);
        var latest = requests[requests.length - 1];
        requests.length === afterLink + 1 && latest.attribute === 'value'
            && latest.configuration.mapping[0] === 0.35 && latest.configuration.display[1] === 0.55;
    )JS")
            .toBool());
    CHECK(fixture.run(R"JS(
        var beforeExplicitApply = requests.length;
        ranges.editRange(a, false, 0.36, 0.54, false);
        var savedWithoutRendering = requests.length === beforeExplicitApply;
        ranges.apply(a);
        savedWithoutRendering && requests.length === beforeExplicitApply + 1
            && requests[requests.length - 1].configuration.mapping[0] === 0.36;
    )JS")
            .toBool());
    CHECK(fixture.run(R"JS(
        ranges.setActive(7, '', -1);
        var beforeCancelEdit = requests.length;
        ranges.editRange(a, false, 0.4, 0.5);
        ranges.ensure(7, 'value', 2, {valid:true, minimum:-5, maximum:5, rangeAvailable:true});
        ranges.ensure(8, 'value', 2, {valid:true, minimum:10, maximum:20, rangeAvailable:true});
        var state = ranges.parameters(a);
        requests.length === beforeCancelEdit && state.mappingLower === 0.4 && state.linked
            && state.minimum === -5 && ranges.entries[ranges.attributeKey(8, 'value', 2)].mappingLower === 10;
    )JS")
            .toBool());
    CHECK(fixture.run(R"JS(
        ranges.removeComponent(7);
        !ranges.entries[a] && !ranges.entries[b]
            && !!ranges.entries[ranges.attributeKey(8, 'value', 2)] && !ranges.activeKeys['7'];
    )JS")
            .toBool());
}
