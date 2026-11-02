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

TEST_CASE("Scalar UI preferences retain identity without storing data metadata", "[scalar-range][qml-state]")
{
    application();
    RangeFixture fixture;
    CHECK(fixture.run(R"JS(
        var original = [0.25, 0.75], requests = [], cancellations = [];
        ranges.renderWindow = {
            scalarRangeInfo: function(component, attribute, type) { return attribute === 'vector' ? [] : original; },
            applyScalarConfig: function(component, attribute, type, mapping, display) {
                requests.push({component:component, attribute:attribute, type:type, mapping:mapping, display:display});
            },
            cancelComponentAttri: function(component) { cancellations.push(component); }
        };
        var key = ranges.ensure(7, 'value', 2);
        var vector = ranges.ensure(7, 'vector', 2);
        var point = ranges.ensure(7, 'value', 0);
        var edit = Object.assign({}, ranges.parameters(key), {mappingLower:0.3, mappingUpper:0.6, linked:true});
        ranges.update(key, edit);
        ranges.update(vector, Object.assign({}, ranges.parameters(vector), {mappingLower:100, mappingUpper:200}));
        requests.length === 0 && key !== point && ranges.configuration(vector).mapping[0] === 100
            && !ranges.rangeInfo(7, 'value', 0).rangeAvailable;
    )JS")
            .toBool());
    CHECK(fixture.run(R"JS(
        ranges.apply(key);
        var next = Object.assign({}, ranges.parameters(key), {mappingLower:0.35, displayLower:0.35, displayUpper:0.6, filtered:true});
        ranges.update(key, next);
        ranges.update(key, Object.assign({}, next, {linked:false}));
        requests.length === 2 && requests[1].mapping[0] === 0.35 && requests[1].display[0] === 0.35;
    )JS")
            .toBool());
    CHECK(fixture.run(R"JS(
        ranges.cancel(7, key, Object.assign({}, ranges.parameters(key), {mappingLower:0.4}));
        ranges.update(key, Object.assign({}, ranges.parameters(key), {mappingUpper:0.5}));
        original = [-5, 5];
        ranges.ensure(7, 'value', 2);
        var state = ranges.parameters(key);
        requests.length === 2 && cancellations.length === 1 && !ranges.activeKeys['7']
            && state.mappingLower === 0.4 && state.mappingUpper === 0.5
            && !('minimum' in state) && !('valid' in state) && ranges.rangeInfo(7, 'value', 2).minimum === -5;
    )JS")
            .toBool());
    CHECK(fixture.run(R"JS(
        ranges.removeComponent(7);
        !ranges.entries[key] && !ranges.entries[vector] && !ranges.entries[point]
            && ranges.parameters(ranges.ensure(7, 'value', 2)).mappingLower === -5;
    )JS")
            .toBool());
}

TEST_CASE("Scalar UI display service shares preparation and keeps explicit actions atomic", "[scalar-range][qml-state]")
{
    application();
    RangeFixture fixture;
    CHECK(fixture.run(R"JS(
        var requests = [], cancellations = [];
        ranges.query = {getComponentAttriInfo: function(component) {return [{name:'value', attrType:2}];}};
        ranges.renderWindow = {
            scalarRangeInfo: function() {return [0.25, 0.75];},
            applyScalarConfig: function(component, attribute, type, mapping, display) {
                requests.push({component:component, attribute:attribute, type:type, mapping:mapping, display:display});
                return false;
            },
            cancelComponentAttri: function(component) {cancellations.push(component);}
        };
        ranges.showAttribute(7, 'value') && !ranges.showAttribute(7, 'missing')
            && requests.length === 1 && requests[0].type === 2;
    )JS")
            .toBool());
    CHECK(fixture.run(R"JS(
        var key = ranges.attributeKey(7, 'value', 2);
        var finalParameters = Object.assign({}, ranges.parameters(key), {mappingLower:0.333333333333});
        ranges.apply(key, finalParameters);
        ranges.cancel(7, key, Object.assign({}, finalParameters, {mappingUpper:0.666666666666}));
        requests.length === 2 && requests[1].mapping[0] === 0.333333333333
            && requests[1].mapping[1] === 0.75 && cancellations.length === 1
            && ranges.parameters(key).mappingUpper === 0.666666666666;
    )JS")
            .toBool());
    CHECK(fixture.run(R"JS(
        ranges.renderWindow = null;
        ranges.apply(key) && ranges.activeKeys['7'] === key && !ranges.showAttribute(7, 'value')
            && requests.length === 2;
    )JS")
            .toBool());
}
