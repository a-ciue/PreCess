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
        var catalog = [{name:'point', attrType:0, componentCount:1}, {name:'value', attrType:2, componentCount:1},
            {name:'vector', attrType:2, componentCount:3}, {name:'point', attrType:2, componentCount:1}];
        ranges.query = {getComponentAttriInfo: function() {return catalog;}};
        ranges.renderWindow = {
            scalarRangeInfo: function(component, attribute) { return attribute === 'vector' ? [] : original; },
            setAttriMode: function(component, attribute, mode, args) {
                requests.push({component:component, attribute:attribute, mode:mode, configuration:args});
            },
            cancelComponentAttri: function(component) { cancellations.push(component); }
        };
        var key = ranges.ensure(7, 'value');
        var vector = ranges.ensure(7, 'vector');
        var point = ranges.ensure(7, catalog[3].name);
        var edit = Object.assign({}, ranges.parameters(key), {mappingLower:0.3, mappingUpper:0.6, linked:true});
        ranges.update(key, edit);
        ranges.update(vector, Object.assign({}, ranges.parameters(vector), {mappingLower:100, mappingUpper:200}));
        ranges.update(point, Object.assign({}, ranges.parameters(point), {filtered:true}));
        requests.length === 0 && key !== point && ranges.configuration(vector).scalar_range[0] === 100
            && point === ranges.attributeKey(7, 'point') && !ranges.rangeInfo(7, catalog[3].name).rangeAvailable
            && !('display_range' in ranges.configuration(point))
            && !('type' in ranges.parameters(point));
    )JS")
            .toBool());
    CHECK(fixture.run(R"JS(
        ranges.apply(key);
        var next = Object.assign({}, ranges.parameters(key), {mappingLower:0.35, displayLower:0.35, displayUpper:0.6, filtered:true});
        ranges.update(key, next);
        ranges.update(key, Object.assign({}, next, {linked:false}));
        requests.length === 2 && requests[1].configuration.scalar_range[0] === 0.35 && requests[1].configuration.display_range[0] === 0.35;
    )JS")
            .toBool());
    CHECK(fixture.run(R"JS(
        ranges.cancel(7, key, Object.assign({}, ranges.parameters(key), {mappingLower:0.4}));
        ranges.update(key, Object.assign({}, ranges.parameters(key), {mappingUpper:0.5}));
        original = [-5, 5];
        ranges.ensure(7, 'value');
        var state = ranges.parameters(key);
        requests.length === 2 && cancellations.length === 1 && !ranges.activeKeys['7']
            && state.mappingLower === 0.4 && state.mappingUpper === 0.5
            && !('minimum' in state) && !('valid' in state) && ranges.rangeInfo(7, 'value').minimum === -5;
    )JS")
            .toBool());
    CHECK(fixture.run(R"JS(
        ranges.removeComponent(7);
        !ranges.entries[key] && !ranges.entries[vector] && !ranges.entries[point]
            && ranges.parameters(ranges.ensure(7, 'value')).mappingLower === -5;
    )JS")
            .toBool());
}

TEST_CASE("Scalar UI display service shares preparation and keeps explicit actions atomic", "[scalar-range][qml-state]")
{
    application();
    RangeFixture fixture;
    CHECK(fixture.run(R"JS(
        var requests = [], cancellations = [];
        ranges.query = {getComponentAttriInfo: function(component) {return [{name:'value', attrType:2, componentCount:1}];}};
        ranges.renderWindow = {
            scalarRangeInfo: function() {return [0.25, 0.75];},
            setAttriMode: function(component, attribute, mode, args) {
                requests.push({component:component, attribute:attribute, mode:mode, configuration:args});
                return false;
            },
            cancelComponentAttri: function(component) {cancellations.push(component);}
        };
        ranges.showAttribute(7, 'value') && !ranges.showAttribute(7, 'missing')
            && requests.length === 1 && requests[0].mode === 1 && !('display_range' in requests[0].configuration);
    )JS")
            .toBool());
    CHECK(fixture.run(R"JS(
        var key = ranges.attributeKey(7, 'value');
        var finalParameters = Object.assign({}, ranges.parameters(key), {mappingLower:0.333333333333});
        ranges.apply(key, finalParameters);
        ranges.cancel(7, key, Object.assign({}, finalParameters, {mappingUpper:0.666666666666}));
        requests.length === 2 && requests[1].configuration.scalar_range[0] === 0.333333333333
            && requests[1].configuration.scalar_range[1] === 0.75 && cancellations.length === 1
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

TEST_CASE("Scalar UI lookup skips unavailable arrays without bypassing valid attributes", "[scalar-range][qml-state]")
{
    application();
    RangeFixture fixture;
    // 空数组和长度不合法的目录项都为零分量，不能遮挡后面的有效同名数组。
    CHECK(fixture.run(R"JS(
        var catalog = [{name:'value', attrType:0, componentCount:0}, {name:'value', attrType:2, componentCount:1}];
        var original = [1, 3];
        ranges.query = {getComponentAttriInfo: function() {return catalog;}};
        ranges.renderWindow = {scalarRangeInfo: function() {return original;}};
        var key = ranges.ensure(7, 'value');
        ranges.save(key, Object.assign({}, ranges.parameters(key), {filtered:true}));
        ranges.attributeInfo(7, 'value').attrType === 2 && ranges.rangeInfo(7, 'value').rangeAvailable
            && ranges.configuration(key).display_range[0] === 1;
    )JS")
            .toBool());
    // 有效多分量点属性仍优先命中，不能为了支持筛选跳到同名面属性。
    CHECK(fixture.run(R"JS(
        catalog[0].componentCount = 3;
        original = [];
        ranges.attributeInfo(7, 'value').attrType === 0 && !ranges.rangeInfo(7, 'value').rangeAvailable
            && !('display_range' in ranges.configuration(key));
    )JS")
            .toBool());
    // 全 NaN 的单分量数组仍有效；空值域不改变同名属性优先级。
    CHECK(fixture.run(R"JS(
        catalog[0].componentCount = 1;
        ranges.attributeInfo(7, 'value').attrType === 0 && !ranges.rangeInfo(7, 'value').valid;
    )JS")
            .toBool());
}
