/** @file TestUndoQml.cpp
 * @brief 实际 QML 删除与选择器失效路径回归测试
 */
#include "ComponentData.h"
#include "FeatureContext.h"
#include "FeatureHandler.h"
#include "FeatureRegistrar.h"
#include "FeatureSystem.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "QFeatureInfo.h"
#include "QModelManager.h"
#include "QModelQuery.h"
#include "QRenderWindow.h"
#include "QSelection.h"
#include "SessionQuery.h"
#include "TreeModel.h"
#include "TreeModelTestHelpers.h"
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickVTKItem.h>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <map>
#include <utility>

Q_IMPORT_QML_PLUGIN(app_corePlugin)
Q_IMPORT_QML_PLUGIN(app_modelPlugin)
Q_IMPORT_QML_PLUGIN(app_renderPlugin)
Q_IMPORT_QML_PLUGIN(app_model_systemsPlugin)
Q_IMPORT_QML_PLUGIN(app_model_systems_algoPlugin)
Q_IMPORT_QML_PLUGIN(app_model_systems_ioPlugin)
Q_IMPORT_QML_PLUGIN(app_model_systems_editPlugin)
Q_IMPORT_QML_PLUGIN(app_model_systems_featurePlugin)

namespace {
class ManySelectors : public systems::feature::FeatureHandler {
public:
    void setup(systems::feature::FeatureRegistrar& reg, systems::feature::FeatureContext&) override
    {
        for (int i = 0; i < 50; ++i)
            reg.addParameter({ ArgTypeEnum::Selector, "目标", "Face", "" });
    }
    std::any execute(systems::feature::FeatureContext& ctx) override
    {
        return ctx.model.addModel("undo fixture", { });
    }
};
QGuiApplication& application()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QT_FORCE_STDERR_LOGGING", "1");
    static int argc = 1;
    static char name[] = "TestUndoQml";
    static char* argv[] = { name, nullptr };
    static QGuiApplication app(argc, argv);
    return app;
}
std::unique_ptr<QObject> load(QQmlEngine& engine, const char* file, const QVariantMap& properties = { })
{
    const QUrl url(QString("qrc:/undo-ui/") + file);
    QQmlComponent component(&engine);
    if (QString(file) == "ObjectTree.qml") {
        QFile source(":" + url.path());
        REQUIRE(source.open(QIODevice::ReadOnly));
        component.setData("import UndoUi 1.0\n" + source.readAll(), url);
    } else {
        component.loadUrl(url);
    }
    INFO(component.errorString().toStdString());
    REQUIRE(component.isReady());
    std::unique_ptr<QObject> object(component.createWithInitialProperties(properties));
    INFO(component.errorString().toStdString());
    REQUIRE(object);
    return object;
}
struct QmlFixture {
    QTemporaryDir directory;
    std::string executable;
    QQmlEngine engine;
    std::unique_ptr<QObject> session;
    std::unique_ptr<QObject> sidebar;
    std::unique_ptr<QObject> tree;
    std::unique_ptr<QObject> viewport;
    QModelManager* manager;
    QObject* app;
    QObject* selection_state;

    QmlFixture()
    {
        static const int tree_type = qmlRegisterType<TreeModel>("UndoUi", 1, 0, "TreeModel");
        static const int theme_type = qmlRegisterSingletonType(QUrl("qrc:/undo-ui/Theme.qml"), "UndoUi", 1, 0, "Theme");
        REQUIRE(directory.isValid());
        executable = (directory.path() + "/isolated/Test.exe").toStdString();
        QModelManager::argv0 = executable;
        engine.rootContext()->setContextProperty("Theme", engine.singletonInstance<QObject*>(theme_type));
        session = load(engine, "OperationSession.qml");
        sidebar = load(engine, "SideBar.qml", { { "session", QVariant::fromValue(session.get()) } });
        tree = load(engine, "ObjectTree.qml");
        viewport = load(engine, "CentralRenderArea.qml");
        manager = engine.singletonInstance<QModelManager*>("app.model", "QModelManager");
        app = engine.singletonInstance<QObject*>("app.core", "App");
        REQUIRE(manager);
        REQUIRE(app);
        selection_state = app->property("selection").value<QObject*>();
        REQUIRE(selection_state);
    }
    ~QmlFixture()
    {
        app->setProperty("activeOperation", QVariant::fromValue(QJSValue(QJSValue::NullValue)));
        QModelManager::argv0 = { };
    }
};

// 结构事件即时同步；这里只泵一次事件循环，让实际 QML 链路完成收尾。
void pumpEvents()
{
    QCoreApplication::processEvents();
}

}

TEST_CASE("Actual scalar editor completes range actions without intermediate requests", "[QML][scalar-range][no-window]")
{
    application();
    QmlFixture f;
    auto ranges = load(f.engine, "ScalarRangeState.qml");
    REQUIRE(f.selection_state->setProperty("activeComponentId", 7));
    auto panel = load(f.engine, "AttributeRenderPanel.qml", { { "rangeState", QVariant::fromValue(ranges.get()) } });
    f.engine.globalObject().setProperty("ranges", f.engine.newQObject(ranges.get()));
    f.engine.globalObject().setProperty("panel", f.engine.newQObject(panel.get()));
    auto setup = f.engine.evaluate(R"JS(
        var requests = [], cancellations = [], histogramRequests = 0;
        var histogramData = {counts: Array(20).fill(1), edges: Array.from({length:21}, (_, i) => i / 20)};
        ranges.query = {
            getComponentAttriInfo: function() {return [{name:'value', attrType:2, componentCount:1}, {name:'vector', attrType:2, componentCount:3}];},
            scalarHistogramInfo: function() { ++histogramRequests; return histogramData; }
        };
        ranges.renderWindow = {
            scalarRangeInfo: function(component) {return component === 8 ? [10, 20] : [0.25, 0.751234567];},
            setAttriMode: function(component, attribute, mode, args) {
                requests.push({component:component, attribute:attribute, mode:mode, configuration:args});
            },
            cancelComponentAttri: function(component) {cancellations.push(component);}
        };
        var key = ranges.ensure(7, 'value');
        panel.attributes = [{name:'value', displayName:'value', typeName:'面', attrType:2, componentCount:1}];
        panel.selectedIndex = 0;
        panel.rangeMetadata = ranges.rangeInfo(7, 'value');
        panel.selectedKey = key;
        ranges.apply(key);
    )JS");
    INFO(setup.toString().toStdString());
    REQUIRE_FALSE(setup.isError());
    // 等待 QML 同一轮事件中安排的失焦提交和直方图刷新。
    auto settle_updates = [] { QCoreApplication::processEvents(QEventLoop::AllEvents, 10); };
    auto* mode = panel->findChild<QObject*>("attributeRenderMode");
    auto* lower = panel->findChild<QObject*>("scalarRangeLower");
    auto* link = panel->findChild<QObject*>("scalarRangeLink");
    REQUIRE(mode);
    REQUIRE(lower);
    REQUIRE(link);
    REQUIRE(mode->setProperty("currentIndex", 1));
    settle_updates();
    REQUIRE(link->property("enabled").toBool());
    REQUIRE(QMetaObject::invokeMethod(link, "clicked"));
    f.engine.evaluate("requests = []");
    auto* editor = panel->findChild<QObject*>("scalarRangeEditor");
    auto* reset = panel->findChild<QObject*>("scalarRangeReset");
    auto* display = panel->findChild<QObject*>("scalarDisplayMode");
    REQUIRE(editor);
    REQUIRE(reset);
    REQUIRE(display);
    CHECK(editor->property("displayMode").toBool());
    auto* mapping = panel->findChild<QObject*>("scalarMappingMode");
    REQUIRE(mapping);
    // 既有范围回归明确选择映射模式，默认模式另在下方验证。
    REQUIRE(QMetaObject::invokeMethod(mapping, "clicked"));
    f.engine.globalObject().setProperty("editor", f.engine.newQObject(editor));
    // 使用生产输入信号，不直接构造输入草稿。
    auto edit_lower = [&](const char* text) {
        REQUIRE(lower->setProperty("text", text));
        REQUIRE(QMetaObject::invokeMethod(lower, "textEdited"));
    };
    auto check = [&](const char* script) {
        auto value = f.engine.evaluate(QString::fromUtf8(script));
        INFO(script);
        INFO(value.toString().toStdString());
        REQUIRE_FALSE(value.isError());
        CHECK(value.toBool());
    };
    SECTION("visible range precedes mapping and arrow keys follow the visual order")
    {
        auto* editor_item = qobject_cast<QQuickItem*>(editor);
        auto* display_item = qobject_cast<QQuickItem*>(display);
        auto* mapping_item = qobject_cast<QQuickItem*>(mapping);
        auto* link_item = qobject_cast<QQuickItem*>(link);
        REQUIRE(editor_item);
        REQUIRE(display_item);
        REQUIRE(mapping_item);
        REQUIRE(link_item);
        editor_item->setWidth(320);
        editor_item->setHeight(editor_item->implicitHeight());
        editor_item->ensurePolished();
        for (auto* child : editor_item->findChildren<QQuickItem*>())
            child->ensurePolished();
        CHECK(display_item->x() < link_item->x());
        CHECK(link_item->x() < mapping_item->x());

        QQuickWindow host;
        auto* panel_item = qobject_cast<QQuickItem*>(panel.get());
        REQUIRE(panel_item);
        panel_item->setParentItem(host.contentItem());
        const auto detach = qScopeGuard([&] { panel_item->setParentItem(nullptr); });
        mapping_item->forceActiveFocus();
        QKeyEvent left(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier);
        QCoreApplication::sendEvent(&host, &left);
        CHECK(display_item->hasActiveFocus());
        CHECK(editor->property("displayMode").toBool());
        QKeyEvent right(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
        QCoreApplication::sendEvent(&host, &right);
        CHECK(mapping_item->hasActiveFocus());
        CHECK_FALSE(editor->property("displayMode").toBool());
        CHECK_FALSE(host.isVisible());
        check("requests.length === 0 && !ranges.parameters(key).filtered");
    }
    SECTION("range labels fields and tooltips share scalar bar precision")
    {
        const double minimum = 0.3777463502849451;
        const double maximum = 0.9998414807984265;
        auto setup = f.engine.evaluate(R"JS(
            ranges.renderWindow.scalarRangeInfo = function() { return [0.3777463502849451,0.9998414807984265]; };
            panel.rangeMetadata = ranges.rangeInfo(7,'value');
            var parameters = Object.assign({},ranges.parameters(key),{
                mappingLower:panel.rangeMetadata.minimum, mappingUpper:panel.rangeMetadata.maximum,
                displayLower:panel.rangeMetadata.minimum, displayUpper:panel.rangeMetadata.maximum
            });
            ranges.save(key,parameters);
            panel.histogramData = {edges:[parameters.mappingLower,parameters.mappingUpper], counts:[1]};
        )JS");
        REQUIRE_FALSE(setup.isError());
        auto* original_lower = panel->findChild<QObject*>("scalarOriginalLower");
        auto* original_upper = panel->findChild<QObject*>("scalarOriginalUpper");
        auto* upper = panel->findChild<QObject*>("scalarRangeUpper");
        REQUIRE(original_lower);
        REQUIRE(original_upper);
        REQUIRE(upper);
        const auto min_text = QString::asprintf("%.3g",minimum);
        const auto max_text = QString::asprintf("%.3g",maximum);
        CHECK(original_lower->property("text").toString() == min_text);
        CHECK(original_upper->property("text").toString() == max_text);
        CHECK(lower->property("text").toString() == min_text);
        CHECK(upper->property("text").toString() == max_text);
        check(R"JS(editor.histogramBinText(0) === '区间：[0.378, 1]\n数量：1')JS");

        // 三位有效数字发生重合时，悬停仍应区分两个实际边界。
        check(R"JS(
            panel.histogramData = {edges:[0.09999,0.1,0.123], counts:[1,2]};
            editor.histogramBinText(0) === '区间：[0.09999, 0.1)\n数量：1'
        )JS");

        // 只编辑一端时，未编辑端仍使用原始精确值。
        edit_lower("0.4");
        REQUIRE(QMetaObject::invokeMethod(editor,"finishEditing"));
        check("requests.length === 1 && requests[0].configuration.scalar_range[1] === 0.9998414807984265");
        CHECK(upper->property("text").toString() == max_text);
    }
    SECTION("folding preserves active filtering and height mode through source refresh")
    {
        auto* toggle = panel->findChild<QObject*>("scalarRangeToggle");
        auto* scale = panel->findChild<QObject*>("scalarHistogramScale");
        auto* summary = panel->findChild<QObject*>("scalarRangeSummary");
        REQUIRE(toggle);
        REQUIRE(scale);
        REQUIRE(summary);
        CHECK(editor->property("expanded").toBool());
        CHECK(toggle->property("text").toString().startsWith(QStringLiteral("▾")));
        REQUIRE(QMetaObject::invokeMethod(display, "clicked"));
        edit_lower("0.4");
        REQUIRE(QMetaObject::invokeMethod(editor, "finishEditing"));
        REQUIRE(QMetaObject::invokeMethod(scale, "toggle"));
        check("requests.length === 1 && ranges.parameters(key).filtered && ranges.parameters(key).linked");
        f.engine.evaluate("var beforeFold = JSON.stringify(ranges.parameters(key)); var beforeConfig = JSON.stringify(ranges.configuration(key)); var beforeActive = JSON.stringify(ranges.activeKeys)");
        REQUIRE(QMetaObject::invokeMethod(toggle, "clicked"));
        settle_updates();
        CHECK_FALSE(editor->property("expanded").toBool());
        CHECK(toggle->property("text").toString().startsWith(QStringLiteral("▸")));
        CHECK(summary->property("visible").toBool());
        CHECK(summary->property("text").toString().contains(QStringLiteral("可见单元 0.4")));
        for (const auto* name : { "scalarHistogramPlot", "scalarHistogramScale", "scalarOriginalRange", "scalarRangeSlider", "scalarRangeLower", "scalarRangeUpper", "scalarMappingMode", "scalarDisplayMode", "scalarRangeLink", "scalarRangeReset" }) {
            auto* control = panel->findChild<QObject*>(name);
            REQUIRE(control);
            CHECK_FALSE(control->property("visible").toBool());
        }
        check("histogramRequests === 1 && requests.length === 1 && cancellations.length === 0"
              " && JSON.stringify(ranges.parameters(key)) === beforeFold && JSON.stringify(ranges.configuration(key)) === beforeConfig"
              " && JSON.stringify(ranges.activeKeys) === beforeActive && editor.displayMode && editor.histogramLogarithmic");
        // 折叠期间更新源数据只刷新元信息，重新展开时才统计最新分布。
        f.engine.evaluate("histogramData = {counts:[2,3,4], edges:[0,1,2,3]}; var queriesBefore = histogramRequests");
        auto* viewport = f.viewport->findChild<QRenderWindow*>();
        REQUIRE(viewport);
        REQUIRE(QMetaObject::invokeMethod(viewport, "scalarRangeChanged", Q_ARG(Index, 7)));
        settle_updates();
        check("histogramRequests === queriesBefore");
        REQUIRE(QMetaObject::invokeMethod(toggle, "clicked"));
        settle_updates();
        CHECK(editor->property("expanded").toBool());
        CHECK_FALSE(summary->property("visible").toBool());
        CHECK(scale->property("visible").toBool());
        CHECK(scale->property("text").toString() == QStringLiteral("对数高度"));
        CHECK(lower->property("visible").toBool());
        CHECK(reset->property("visible").toBool());
        check("JSON.stringify(editor.histogramCounts) === '[2,3,4]' && requests.length === 1"
              " && JSON.stringify(ranges.parameters(key)) === beforeFold && JSON.stringify(ranges.configuration(key)) === beforeConfig"
              " && JSON.stringify(ranges.activeKeys) === beforeActive && editor.displayMode && editor.histogramLogarithmic");
    }
    SECTION("folding keeps invalid drafts and applying reveals their error")
    {
        QQuickWindow host;
        auto* panel_item = qobject_cast<QQuickItem*>(panel.get());
        auto* lower_item = qobject_cast<QQuickItem*>(lower);
        auto* toggle = panel->findChild<QQuickItem*>("scalarRangeToggle");
        REQUIRE(panel_item);
        REQUIRE(lower_item);
        REQUIRE(toggle);
        // 隐藏窗口提供真实的焦点上下文，整个测试不显示窗口。
        panel_item->setParentItem(host.contentItem());
        const auto detach = qScopeGuard([&] { panel_item->setParentItem(nullptr); });
        lower_item->forceActiveFocus();
        REQUIRE(lower_item->hasActiveFocus());
        edit_lower("0.333333333333");
        REQUIRE(QMetaObject::invokeMethod(toggle, "clicked"));
        REQUIRE(toggle->hasActiveFocus());
        CHECK_FALSE(lower_item->hasActiveFocus());
        settle_updates();
        CHECK_FALSE(editor->property("expanded").toBool());
        check("requests.length === 1 && ranges.parameters(key).mappingLower === 0.333333333333 && !editor.draft");

        REQUIRE(QMetaObject::invokeMethod(toggle, "clicked"));
        lower_item->forceActiveFocus();
        REQUIRE(lower_item->hasActiveFocus());
        edit_lower("-");
        REQUIRE(QMetaObject::invokeMethod(toggle, "clicked"));
        settle_updates();
        CHECK_FALSE(editor->property("expanded").toBool());
        CHECK_FALSE(host.isVisible());
        auto* apply = panel->findChild<QObject*>("attributeRenderApply");
        REQUIRE(apply);
        REQUIRE(apply->property("enabled").toBool());
        REQUIRE(QMetaObject::invokeMethod(apply, "clicked"));
        CHECK(editor->property("expanded").toBool());
        CHECK(lower->property("visible").toBool());
        CHECK(lower->property("text").toString() == "-");
        check("requests.length === 1 && editor.draft !== null && editor.rangeError.length > 0");
    }
    SECTION("folding returns height to the framed attribute list")
    {
        double panel_width = 360.0;
        SECTION("narrow panel") { panel_width = 280.0; }
        SECTION("regular panel") { }
        auto* panel_item = qobject_cast<QQuickItem*>(panel.get());
        auto* list = panel->findChild<QQuickItem*>("attributeRenderList");
        auto* frame = panel->findChild<QQuickItem*>("attributeRenderListFrame");
        auto* toggle = panel->findChild<QObject*>("scalarRangeToggle");
        auto* apply = panel->findChild<QQuickItem*>("attributeRenderApply");
        auto* cancel = panel->findChild<QQuickItem*>("attributeRenderCancel");
        REQUIRE(panel_item);
        REQUIRE(list);
        REQUIRE(frame);
        REQUIRE(toggle);
        REQUIRE(apply);
        REQUIRE(cancel);
        panel_item->setWidth(panel_width);
        panel_item->setHeight(700.0);
        // 无窗口测试显式完成父子布局的 polish，使检查针对实际分配的空间。
        auto settle_layout = [&] {
            for (int pass = 0; pass < 2; ++pass) {
                for (auto* item : panel->findChildren<QQuickItem*>())
                    item->ensurePolished();
                QCoreApplication::processEvents();
            }
        };
        settle_layout();
        REQUIRE(list->height() > 0);
        const double expanded_height = list->height();
        const double footer_y = apply->mapToItem(panel_item, QPointF()).y();
        CHECK(list->width() < frame->width());
        REQUIRE(QMetaObject::invokeMethod(toggle, "clicked"));
        settle_layout();
        CHECK(list->height() > expanded_height + 48);
        CHECK(apply->isVisible());
        CHECK(cancel->isVisible());
        CHECK(std::abs(apply->mapToItem(panel_item, QPointF()).y() - footer_y) < 1e-6);
        REQUIRE(QMetaObject::invokeMethod(toggle, "clicked"));
        settle_layout();
        CHECK(std::abs(list->height() - expanded_height) < 1e-6);
        check("requests.length === 0 && cancellations.length === 0");
    }
    SECTION("histogram follows source data only while the panel is visible")
    {
        auto* plot = panel->findChild<QQuickItem*>("scalarHistogramPlot");
        auto* scale = panel->findChild<QObject*>("scalarHistogramScale");
        auto* viewport = f.viewport->findChild<QRenderWindow*>();
        REQUIRE(plot);
        REQUIRE(scale);
        REQUIRE(viewport);
        CHECK_FALSE(scale->property("checked").toBool());
        CHECK(plot->isVisible());
        check("histogramRequests === 1 && editor.histogramCounts.length === 20");
        check(R"JS(editor.histogramBinText(0) === '区间：[0, 0.05)\n数量：1'
            && editor.histogramBinText(19) === '区间：[0.95, 1]\n数量：1')JS");

        REQUIRE(QMetaObject::invokeMethod(display, "clicked"));
        edit_lower("0.4");
        REQUIRE(QMetaObject::invokeMethod(editor, "finishEditing"));
        f.engine.evaluate("panel.refreshRangeMetadata(); panel.applySelectedAttribute(); panel.applySelectedAttribute()");
        REQUIRE(QMetaObject::invokeMethod(viewport, "scalarRangeChanged", Q_ARG(Index, 8)));
        settle_updates();
        check("histogramRequests === 1");

        // 挂在场景树内验证整面板显隐，保留子项继承的真实可见性。
        QQuickItem parent_item;
        auto* panel_item = qobject_cast<QQuickItem*>(panel.get());
        REQUIRE(panel_item);
        panel_item->setParentItem(&parent_item);
        const auto detach = qScopeGuard([&] { panel_item->setParentItem(nullptr); });
        panel_item->setSize(QSizeF(320, 600));
        panel_item->setVisible(false);
        f.engine.evaluate("histogramData = {counts:[0,1,1000000], edges:[0,0.25,0.5,0.75]}; var queriesBefore = histogramRequests");
        REQUIRE(QMetaObject::invokeMethod(viewport, "scalarRangeChanged", Q_ARG(Index, 7)));
        settle_updates();
        check("histogramRequests === queriesBefore");
        CHECK_FALSE(plot->isVisible());
        panel_item->setVisible(true);
        settle_updates();
        CHECK(plot->isVisible());
        check("JSON.stringify(editor.histogramCounts) === '[0,1,1000000]'");
        // 隐藏窗口下没有场景帧调度，显式分配图表所在的布局空间。
        panel->findChild<QQuickItem*>("attributeRenderLayout")->ensurePolished();
        qobject_cast<QQuickItem*>(editor)->ensurePolished();
        REQUIRE(plot->height() > 0);
        REQUIRE_FALSE(plot->childItems().isEmpty());
        for (auto* bar : plot->childItems().front()->childItems()) {
            const auto count = bar->property("modelData");
            if (count.isValid())
                CHECK(bar->height() == (count.toDouble() == 0 ? 0 : count.toDouble() == 1 ? 2
                                                                                          : plot->height()));
        }
        f.engine.evaluate("queriesBefore = histogramRequests");
        REQUIRE(QMetaObject::invokeMethod(scale, "toggle"));
        CHECK(scale->property("checked").toBool());
        check("histogramRequests === queriesBefore && editor.histogramBinText(1).endsWith('数量：1')");
        for (auto* bar : plot->childItems().front()->childItems()) {
            if (bar->property("modelData").toDouble() == 1)
                CHECK(bar->height() > 2);
        }
        // 窄箱用偏移显示共享边界，小于 0.0001 的刻度使用科学计数法。
        f.engine.evaluate("histogramData = {counts:[1,2], edges:[0,0.000025,0.00005], origin:1.234567}");
        REQUIRE(QMetaObject::invokeMethod(viewport, "scalarRangeChanged", Q_ARG(Index, 7)));
        settle_updates();
        check(R"JS(editor.histogramBinText(0) === '相对 1.234567 的偏移：[0, 2.5e-05)\n数量：1'
            && editor.histogramBinText(1) === '相对 1.234567 的偏移：[2.5e-05, 5e-05]\n数量：2')JS");

        f.engine.evaluate("histogramData = {}");
        REQUIRE(QMetaObject::invokeMethod(viewport, "scalarRangeChanged", Q_ARG(Index, 7)));
        settle_updates();
        CHECK_FALSE(plot->isVisible());
    }
    SECTION("text input clamps both range modes to the precise source limits")
    {
        auto* upper = panel->findChild<QObject*>("scalarRangeUpper");
        REQUIRE(upper);
        REQUIRE(QMetaObject::invokeMethod(link, "clicked"));
        f.engine.evaluate("requests = []");
        edit_lower("-100");
        REQUIRE(upper->setProperty("text", "0.6"));
        REQUIRE(QMetaObject::invokeMethod(upper, "textEdited"));
        REQUIRE(QMetaObject::invokeMethod(editor, "finishEditing"));
        check("requests.length === 1 && JSON.stringify(requests[0].configuration.scalar_range) === '[0.25,0.6]'");

        REQUIRE(QMetaObject::invokeMethod(display, "clicked"));
        edit_lower("0.4");
        REQUIRE(upper->setProperty("text", "10000"));
        REQUIRE(QMetaObject::invokeMethod(upper, "textEdited"));
        REQUIRE(QMetaObject::invokeMethod(editor, "finishEditing"));
        check("requests.length === 2 && JSON.stringify(requests[1].configuration.display_range) === '[0.4,0.751234567]'"
              " && JSON.stringify(requests[1].configuration.scalar_range) === '[0.25,0.6]'");

        // 已保存的宽色标只限幅本次键入的一端。
        REQUIRE(QMetaObject::invokeMethod(mapping, "clicked"));
        auto saved = f.engine.evaluate(R"JS(
            ranges.save(key, Object.assign({}, ranges.parameters(key), {mappingLower:-1, mappingUpper:2}));
            requests = [];
        )JS");
        REQUIRE_FALSE(saved.isError());
        edit_lower("-1");
        REQUIRE(QMetaObject::invokeMethod(editor, "finishEditing"));
        check("requests.length === 1 && JSON.stringify(requests[0].configuration.scalar_range) === '[0.25,2]'");
    }
    SECTION("linked reset discards draft and restores both ranges")
    {
        edit_lower("0.3");
        REQUIRE(QMetaObject::invokeMethod(editor, "finishEditing"));
        f.engine.evaluate("requests = []");
        edit_lower("0.4");
        REQUIRE(QMetaObject::invokeMethod(reset, "clicked"));
        QCoreApplication::processEvents();
        check("requests.length === 1 && ranges.parameters(key).linked && !ranges.parameters(key).filtered"
              " && ranges.parameters(key).mappingLower === 0.25 && ranges.parameters(key).displayLower === 0.25");
    }
    SECTION("independent reset keeps the other range")
    {
        REQUIRE(QMetaObject::invokeMethod(link, "clicked"));
        edit_lower("0.3");
        REQUIRE(QMetaObject::invokeMethod(editor, "finishEditing"));
        REQUIRE(QMetaObject::invokeMethod(display, "clicked"));
        edit_lower("0.4");
        REQUIRE(QMetaObject::invokeMethod(editor, "finishEditing"));
        f.engine.evaluate("requests = []");
        REQUIRE(QMetaObject::invokeMethod(reset, "clicked"));
        check("requests.length === 1 && !ranges.parameters(key).linked && !ranges.parameters(key).filtered"
              " && ranges.parameters(key).mappingLower === 0.3 && ranges.parameters(key).displayLower === 0.25");
    }
    SECTION("explicit apply publishes only the final linked parameters")
    {
        edit_lower("0.333333333333");
        auto* apply = panel->findChild<QObject*>("attributeRenderApply");
        REQUIRE(apply);
        REQUIRE(QMetaObject::invokeMethod(apply, "clicked"));
        QCoreApplication::processEvents();
        check("requests.length === 1 && requests[0].configuration.scalar_range[0] === 0.333333333333"
              " && requests[0].configuration.display_range[0] === 0.333333333333");
    }
    SECTION("cancel saves parameters without applying and reapply restores them")
    {
        edit_lower("0.333333333333");
        auto* cancel = panel->findChild<QObject*>("attributeRenderCancel");
        REQUIRE(cancel);
        REQUIRE(QMetaObject::invokeMethod(cancel, "clicked"));
        QCoreApplication::processEvents();
        check("requests.length === 0 && cancellations.length === 1 && cancellations[0] === 7"
              " && !ranges.activeKeys['7'] && ranges.parameters(key).mappingLower === 0.333333333333");
        REQUIRE(QMetaObject::invokeMethod(panel.get(), "applySelectedAttribute"));
        check("requests.length === 1 && requests[0].configuration.display_range[0] === 0.333333333333");
    }
    SECTION("pending drag and late old gesture retain their captured property")
    {
        auto result = f.engine.evaluate(R"JS(
            editor.handleEditTarget = {key:key, display:false};
            editor.queueRange(0.3, 0.6);
            var other = ranges.ensure(8, 'value');
            panel.rangeMetadata = ranges.rangeInfo(8, 'value');
            panel.selectedKey = other;
            editor.queueRange(0.4, 0.5);
        )JS");
        REQUIRE_FALSE(result.isError());
        QCoreApplication::processEvents();
        check("requests.length === 1 && requests[0].component === 7 && ranges.parameters(key).mappingLower === 0.3"
              " && ranges.parameters(other).mappingLower === 10 && ranges.parameters(other).mappingUpper === 20");
    }
    SECTION("multicomponent optional mapping retains precise endpoints")
    {
        auto value = f.engine.evaluate(R"JS(
            ranges.renderWindow.scalarRangeInfo = function() {return [];};
            var vector = ranges.ensure(7, 'vector');
            panel.rangeMetadata = ranges.rangeInfo(7, 'vector');
            panel.selectedKey = vector;
        )JS");
        REQUIRE_FALSE(value.isError());
        edit_lower("100.123456789");
        REQUIRE(QMetaObject::invokeMethod(editor, "finishEditing"));
        check("ranges.parameters(vector).mappingLower === 100.123456789 && ranges.parameters(vector).mappingUpper === null"
              " && requests.length === 0");
        auto* upper = panel->findChild<QObject*>("scalarRangeUpper");
        REQUIRE(upper);
        REQUIRE(upper->setProperty("text", "200.987654321"));
        REQUIRE(QMetaObject::invokeMethod(upper, "textEdited"));
        REQUIRE(QMetaObject::invokeMethod(editor, "finishEditing"));
        check("ranges.apply(vector) && requests.length === 1 && requests[0].configuration.scalar_range[0] === 100.123456789"
              " && requests[0].configuration.scalar_range[1] === 200.987654321 && !('display_range' in requests[0].configuration)");
        CHECK_FALSE(link->property("enabled").toBool());
        CHECK_FALSE(editor->property("displayMode").toBool());
    }
    SECTION("metadata refresh preserves draft and reset uses the latest original range")
    {
        edit_lower("0.333333333333");
        auto value = f.engine.evaluate(R"JS(
            ranges.renderWindow.scalarRangeInfo = function() {return [-5, 5];};
            panel.rangeMetadata = ranges.rangeInfo(7, 'value');
            ranges.ensure(7, 'value', panel.rangeMetadata);
        )JS");
        REQUIRE_FALSE(value.isError());
        CHECK(lower->property("text").toString() == "0.333333333333");
        REQUIRE(QMetaObject::invokeMethod(editor, "finishEditing"));
        check("requests.length === 1 && ranges.parameters(key).mappingLower === 0.333333333333"
              " && ranges.parameters(key).mappingUpper === 0.751234567");
        f.engine.evaluate("requests = []");
        REQUIRE(QMetaObject::invokeMethod(reset, "clicked"));
        check("requests.length === 1 && ranges.parameters(key).mappingLower === -5 && ranges.parameters(key).mappingUpper === 5"
              " && ranges.parameters(key).displayLower === -5 && !ranges.parameters(key).filtered");
    }
    SECTION("unlink merges the final draft once")
    {
        SECTION("color mapping draft") { }
        SECTION("visible cells draft")
        {
            auto* display = panel->findChild<QObject*>("scalarDisplayMode");
            REQUIRE(display);
            REQUIRE(QMetaObject::invokeMethod(display, "clicked"));
        }
        // 触发生产输入框和按钮的处理链，不直接构造草稿，也不创建或显示窗口。
        REQUIRE(lower->setProperty("text", "0.333333333333"));
        REQUIRE(QMetaObject::invokeMethod(lower, "textEdited"));
        REQUIRE(QMetaObject::invokeMethod(link, "clicked"));
        QCoreApplication::processEvents();
        auto result = f.engine.evaluate(R"JS(
            var state = ranges.parameters(key);
            var config = requests.length === 1 ? requests[0].configuration : null;
            !state.linked && !panel.currentRange.linked && requests.length === 1
                && requests[0].component === 7 && requests[0].attribute === 'value' && requests[0].mode === 1
                && config.scalar_range[0] === 0.333333333333 && config.display_range[0] === 0.333333333333
                && config.scalar_range[1] === 0.751234567 && config.display_range[1] === 0.751234567;
        )JS");
        INFO(result.toString().toStdString());
        REQUIRE_FALSE(result.isError());
        CHECK(result.toBool());
    }
    auto* panel_item = qobject_cast<QQuickItem*>(panel.get());
    REQUIRE(panel_item);
    CHECK(panel_item->window() == nullptr);
}

// 使用实际编辑器验证原始分布的横轴与滑条跨度无关。
TEST_CASE("Actual scalar histogram fills its plot and centers a constant", "[QML][scalar-range][histogram][no-window]")
{
    application();
    QmlFixture f;
    bool constant = false;
    int bin_count = 32;
    bool unequal_ends = false;
    bool extreme = false;
    SECTION("original distribution remains readable with a wide saved mapping") { }
    SECTION("a single nonconstant bin still fills the plot") { bin_count = 1; }
    SECTION("constant is drawn as a single centered column") { constant = true; }
    SECTION("short end bins use their actual widths and hover intervals") { unequal_ends = true; }
    SECTION("opposite finite extremes have finite plot coordinates") { extreme = true; }
    QVariantList edges, counts;
    if (constant) {
        edges = { 0.12345, 0.12345 };
        counts = { 5 };
    } else if (unequal_ends) {
        edges = { 0.0123, 0.05, 0.1, 0.1234 };
        counts = { 1, 2, 3 };
    } else if (extreme) {
        const double largest = std::numeric_limits<double>::max();
        edges = { -largest, 0.0, largest };
        counts = { 1, 1 };
    } else {
        for (int i = 0; i < bin_count; ++i) {
            edges.append(static_cast<double>(i) / bin_count);
            counts.append(1);
        }
        edges.append(1.0);
    }
    const QVariantMap histogram { { "edges", edges }, { "counts", counts } };
    const QVariantMap parameters { { "mappingLower", 0.0 }, { "mappingUpper", 10000.0 } };
    const QVariantMap metadata { { "valid", true }, { "rangeAvailable", true },
        { "minimum", constant ? QVariant(0.12345) : edges.front() },
        { "maximum", constant ? QVariant(0.12345) : edges.back() } };
    auto editor = load(f.engine, "ScalarRangeEditor.qml", { { "parameters", parameters }, { "metadata", metadata }, { "histogramData", histogram }, { "displayMode", false }, { "width", 320.0 } });
    auto* item = qobject_cast<QQuickItem*>(editor.get());
    REQUIRE(item);
    item->setHeight(item->implicitHeight());
    item->ensurePolished();
    pumpEvents();
    auto* plot = editor->findChild<QQuickItem*>("scalarHistogramPlot");
    REQUIRE(plot);
    REQUIRE(plot->width() > 200);
    REQUIRE_FALSE(plot->childItems().isEmpty());
    auto* bars = plot->childItems().front();
    if (constant) {
        CHECK(bars->width() == 8);
        CHECK(bars->x() + bars->width() / 2 == plot->width() / 2);
    } else {
        CHECK(editor->property("sliderMaximum").toDouble() == (extreme ? std::numeric_limits<double>::max() : 10000.0));
        CHECK(bars->width() == plot->width());
        auto* first_bar = bars->childItems().front();
        CHECK(first_bar->width() >= 5);
        CHECK(first_bar->height() > 0);
        if (unequal_ends) {
            CHECK(editor->findChild<QObject*>("scalarOriginalLower")->property("text").toString() == "0.0123");
            CHECK(editor->findChild<QObject*>("scalarOriginalUpper")->property("text").toString() == "0.123");
            auto* middle_bar = bars->childItems()[1];
            auto* last_bar = bars->childItems()[2];
            CHECK(first_bar->width() < middle_bar->width());
            CHECK(last_bar->width() < middle_bar->width());
            CHECK(std::abs(middle_bar->x() / bars->width() - (0.05 - 0.0123) / (0.1234 - 0.0123)) < 1e-12);
            f.engine.globalObject().setProperty("editor", f.engine.newQObject(editor.get()));
            auto hover = f.engine.evaluate(R"JS(
                editor.histogramBinAt(editor.histogramEdgePosition(1) - 0.001) === 0
                && editor.histogramBinAt(editor.histogramEdgePosition(1) + 0.001) === 1
                && editor.histogramBinAt(1) === 2
                && editor.histogramBinText(0) === '区间：[0.0123, 0.05)\n数量：1'
                && editor.histogramBinText(2) === '区间：[0.1, 0.123]\n数量：3'
            )JS");
            REQUIRE_FALSE(hover.isError());
            CHECK(hover.toBool());
        }
        if (extreme)
            CHECK(std::abs(first_bar->width() / bars->width() - 0.45) < 1e-12);
    }
    CHECK(item->window() == nullptr);
}

TEST_CASE("Scalar UI skips unrenderable named attributes and applies the matching face range", "[QML][scalar-range][no-window]")
{
    application();
    ModelLayer model;
    auto component = std::make_unique<ComponentData>();
    component->mesh = std::make_unique<MeshData>();
    auto& mesh = *component->mesh;
    mesh.vertex_positions_ = { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 1, 0 }, { 0, 1, 0 } };
    mesh.vertex_count_ = 4;
    mesh.face_vertices_ = { 0, 1, 2, 0, 2, 3 };
    mesh.face_vertices_offset_ = { 0, 3, 6 };
    mesh.face_attributes_["value"] = { 0.25, 0.75 };
    mesh.vertex_attributes_["value"] = { };
    SECTION("empty point attribute") { }
    SECTION("point attribute length does not match the point count")
    {
        mesh.vertex_attributes_["value"] = { 1.0 };
    }
    ComponentDatas components;
    components.push_back(std::move(component));
    const auto model_id = model.addModel("scalar lookup", std::move(components));
    const auto component_id = model.modelById(model_id)->componentIds().front();
    session::SessionQuery source_query(model);
    QModelQuery query(&source_query);
    const auto catalog = query.getComponentAttriInfo(component_id);
    REQUIRE(catalog.size() == 2);
    CHECK(catalog[0].toMap().value("componentCount").toInt() == 0);
    CHECK(catalog[1].toMap().value("componentCount").toInt() == 1);
    // 无需创建渲染窗口即可从模型查询统计；显示筛选不能改变原始分布。
    const auto histogram = query.scalarHistogramInfo(component_id, "value");
    const auto counts = histogram.value("counts").toList();
    REQUIRE_FALSE(counts.empty());
    CHECK(counts.front().toLongLong() == 1);
    CHECK(counts.back().toLongLong() == 1);
    CHECK(histogram.value("edges").toList().front().toDouble() == 0.25);
    CHECK(histogram.value("edges").toList().back().toDouble() == 0.75);
    CHECK(query.scalarHistogramInfo(component_id, "missing").isEmpty());
    QRenderWindow viewport;
    const auto data = query.getMeshDataByComponent(component_id);
    REQUIRE(data);
    viewport.loadMeshData(component_id, *data);
    QQmlEngine engine;
    auto ranges = load(engine, "ScalarRangeState.qml", { { "query", QVariant::fromValue(&query) }, { "renderWindow", QVariant::fromValue(&viewport) } });
    engine.globalObject().setProperty("ranges", engine.newQObject(ranges.get()));
    engine.globalObject().setProperty("componentId", component_id);
    // 真实目录、界面能力和参数入口保持一致；只验证接受配置，不创建渲染场景。
    auto applied = engine.evaluate(R"JS(
        var metadata = ranges.rangeInfo(componentId, 'value');
        var key = ranges.ensure(componentId, 'value');
        ranges.save(key, Object.assign({}, ranges.parameters(key), {
            displayLower:0.5, displayUpper:1, filtered:true
        }));
        var config = ranges.configuration(key);
        metadata.rangeAvailable && metadata.minimum === 0.25 && metadata.maximum === 0.75
            && ranges.attributeInfo(componentId, 'value').attrType === 2
            && JSON.stringify(config.display_range) === '[0.5,1]'
            && ranges.renderWindow.setAttriMode(componentId, 'value', 1, config);
    )JS");
    INFO(applied.toString().toStdString());
    REQUIRE_FALSE(applied.isError());
    REQUIRE(applied.toBool());

    CHECK(query.scalarHistogramInfo(component_id, "value") == histogram);
    CHECK(viewport.window() == nullptr);
}

TEST_CASE("Actual QML synchronizes reversed mesh and geometry visibility across tree refresh", "[QML][visibility]")
{
    application();
    QmlFixture f;
    auto& model = *f.manager->getModelManager();
    model.setUndoRecorder(nullptr);
    ComponentDatas components;
    auto component = std::make_unique<ComponentData>();
    component->mesh = std::make_unique<MeshData>();
    component->geometry = std::make_unique<GeometryData>();
    components.push_back(std::move(component));
    const Index model_id = model.addModel("visibility fixture", std::move(components));
    const Index component_id = model.modelById(model_id)->componentIds()[0];
    auto* tree = f.tree->findChild<TreeModel*>();
    auto* viewport = f.viewport->findChild<QQuickVTKItem*>();
    REQUIRE(tree);
    REQUIRE(viewport);
    REQUIRE(tree->refresh());

    for (auto [mesh_visible, geometry_visible] : { std::make_pair(true, false),
             std::make_pair(false, false), std::make_pair(false, true), std::make_pair(true, true) }) {
        // 触发实际视口信号，覆盖 CentralRenderArea 到 ObjectTree 的回传链路。
        REQUIRE(QMetaObject::invokeMethod(viewport, "componentVisibilityChanged",
            Q_ARG(Index, component_id), Q_ARG(bool, mesh_visible), Q_ARG(bool, geometry_visible)));
        for (int round = 0; round < 2; ++round) {
            const auto index = tree->findIndexByNodeId(component_id, 1);
            REQUIRE(index.isValid());
            CHECK(tree->data(index, TreeModel::IsVisibleRole).toBool() == (mesh_visible || geometry_visible));
            CHECK(tree->data(tree->parent(index), TreeModel::IsVisibleRole).toBool() == (mesh_visible || geometry_visible));
            REQUIRE(tree->rowCount(index) == 2);
            for (int i = 0; i < tree->rowCount(index); ++i) {
                const auto child = tree->index(i, 0, index);
                const bool is_mesh = tree->data(child, TreeModel::NodeTypeRole).toInt() == static_cast<int>(TreeNode::NodeType::Mesh);
                CHECK(tree->data(child, TreeModel::IsVisibleRole).toBool() == (is_mesh ? mesh_visible : geometry_visible));
            }
            // 树重建后仍需保留各子节点状态，不能退回统一的组件开关。
            REQUIRE(tree->refresh());
        }
    }
}

TEST_CASE("Actual QML keeps reported visibility when a subtype re-enters the tree", "[QML][visibility]")
{
    application();
    QmlFixture f;
    auto& model = *f.manager->getModelManager();
    model.setUndoRecorder(nullptr);
    ComponentDatas components;
    auto component = std::make_unique<ComponentData>();
    component->mesh = std::make_unique<MeshData>();
    component->geometry = std::make_unique<GeometryData>();
    components.push_back(std::move(component));
    const Index model_id = model.addModel("visibility restore", std::move(components));
    const Index component_id = model.modelById(model_id)->componentIds()[0];
    auto* tree = f.tree->findChild<TreeModel*>();
    auto* viewport = f.viewport->findChild<QQuickVTKItem*>();
    REQUIRE(tree);
    REQUIRE(viewport);
    REQUIRE(tree->refresh());

    const auto report = [&](bool mesh_visible, bool geometry_visible) {
        REQUIRE(QMetaObject::invokeMethod(viewport, "componentVisibilityChanged",
            Q_ARG(Index, component_id), Q_ARG(bool, mesh_visible), Q_ARG(bool, geometry_visible)));
    };
    const auto subtype_visible = [&](TreeNode::NodeType type) {
        const QModelIndex component_index = tree->findIndexByNodeId(component_id, 1);
        REQUIRE(component_index.isValid());
        for (int i = 0; i < tree->rowCount(component_index); ++i) {
            const QModelIndex child = tree->index(i, 0, component_index);
            if (tree->data(child, TreeModel::NodeTypeRole).toInt() == static_cast<int>(type))
                return tree->data(child, TreeModel::IsVisibleRole).toBool();
        }
        return false;
    };

    // 隐藏几何：完整二元结果一次应用。
    report(true, false);
    CHECK(subtype_visible(TreeNode::NodeType::Mesh));
    CHECK_FALSE(subtype_visible(TreeNode::NodeType::Geometry));

    // 删除网格并等待结构同步完成，树中只剩隐藏的几何。
    auto component_operator = model.getComponentOperator(component_id);
    REQUIRE(component_operator);
    component_operator->removeMesh();
    model.flushNotifications();
    REQUIRE(waitForCondition([&] {
        const QModelIndex idx = tree->findIndexByNodeId(component_id, 1);
        return idx.isValid() && !tree->data(idx, TreeModel::IsVisibleRole).toBool();
    }));
    CHECK_FALSE(tree->data(tree->findIndexByNodeId(component_id, 1), TreeModel::IsVisibleRole).toBool());

    // 撤销恢复：结构同步尚未执行时回传"网格可见、几何隐藏"，创建后不得被默认值覆盖。
    component_operator->replaceMesh(std::make_unique<MeshData>());
    model.flushNotifications();
    report(true, false);
    REQUIRE(waitForCondition([&] {
        const QModelIndex idx = tree->findIndexByNodeId(component_id, 1);
        if (!idx.isValid())
            return false;
        for (int i = 0; i < tree->rowCount(idx); ++i) {
            const QModelIndex child = tree->index(i, 0, idx);
            if (tree->data(child, TreeModel::NodeTypeRole).toInt() == static_cast<int>(TreeNode::NodeType::Mesh))
                return tree->data(child, TreeModel::IsVisibleRole).toBool();
        }
        return false;
    }));

    CHECK(subtype_visible(TreeNode::NodeType::Mesh));
    CHECK_FALSE(subtype_visible(TreeNode::NodeType::Geometry));
    CHECK(tree->data(tree->findIndexByNodeId(component_id, 1), TreeModel::IsVisibleRole).toBool());
}

TEST_CASE("Actual QML applies hidden state reported before a component enters the tree", "[QML][visibility]")
{
    application();
    QmlFixture f;
    auto& model = *f.manager->getModelManager();
    model.setUndoRecorder(nullptr);
    ComponentDatas components;
    components.push_back(std::make_unique<ComponentData>());
    const Index model_id = model.addModel("pending component", std::move(components));
    auto* tree = f.tree->findChild<TreeModel*>();
    REQUIRE(tree);
    REQUIRE(tree->refresh());

    // 组件已存在于模型与视口，但合并刷新尚未把它加入对象树。
    auto model_operator = model.getModelOperator(model_id);
    REQUIRE(model_operator);
    auto created = std::make_unique<ComponentData>();
    created->geometry = std::make_unique<GeometryData>();
    const Index component_id = model_operator->addGeometryComponent(std::move(created));
    REQUIRE_FALSE(tree->findIndexByNodeId(component_id, 1).isValid());

    REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "hideAllNodes"));
    REQUIRE(waitForCondition([&] {
        const QModelIndex idx = tree->findIndexByNodeId(component_id, 1);
        return idx.isValid() && !tree->data(idx, TreeModel::IsVisibleRole).toBool();
    }));

    const QModelIndex component_index = tree->findIndexByNodeId(component_id, 1);
    REQUIRE(component_index.isValid());
    CHECK_FALSE(tree->data(component_index, TreeModel::IsVisibleRole).toBool());
    REQUIRE(tree->rowCount(component_index) == 1);
    CHECK_FALSE(tree->data(tree->index(0, 0, component_index), TreeModel::IsVisibleRole).toBool());
}

TEST_CASE("Actual QML refreshes component tri-state when one subtype changes", "[QML][visibility]")
{
    application();
    QmlFixture f;
    auto& model = *f.manager->getModelManager();
    model.setUndoRecorder(nullptr);
    ComponentDatas components;
    auto component = std::make_unique<ComponentData>();
    component->mesh = std::make_unique<MeshData>();
    component->geometry = std::make_unique<GeometryData>();
    component->geometry->setRootShape(GeometryBuilder::makeBox(0.0, 0.0, 0.0, 1.0, 1.0, 1.0));
    components.push_back(std::move(component));
    const Index model_id = model.addModel("tri-state fixture", std::move(components));
    const Index component_id = model.modelById(model_id)->componentIds().front();

    auto* tree = f.tree->findChild<TreeModel*>();
    auto* viewport = f.viewport->findChild<QRenderWindow*>();
    REQUIRE(tree);
    REQUIRE(viewport);
    REQUIRE(tree->refresh());
    viewport->onComponentChanged(component_id);

    const QModelIndex component_index = tree->findIndexByNodeId(component_id, 1);
    REQUIRE(component_index.isValid());
    int visibility_updates = 0;
    QObject::connect(tree, &QAbstractItemModel::dataChanged, tree,
        [&](const QModelIndex& first, const QModelIndex&, const QList<int>& roles) {
            if (first == component_index && roles.contains(TreeModel::VisibilityStateRole))
                ++visibility_updates;
        });

    // 只隐藏几何：组件布尔显隐仍为可见，但三态必须变为部分显示并通知组件行。
    viewport->setGeometryVisibility(component_id, false);
    CHECK(tree->data(component_index, TreeModel::IsVisibleRole).toBool());
    CHECK(tree->data(component_index, TreeModel::VisibilityStateRole).toInt() == Qt::PartiallyChecked);
    CHECK(visibility_updates >= 1);
}

TEST_CASE("Actual QML keeps all show/hide state consistent across repeated toggles", "[QML][visibility]")
{
    application();
    QmlFixture f;
    auto& model = *f.manager->getModelManager();
    model.setUndoRecorder(nullptr);

    ComponentDatas components;
    auto mesh_geometry = std::make_unique<ComponentData>();
    mesh_geometry->mesh = std::make_unique<MeshData>();
    mesh_geometry->geometry = std::make_unique<GeometryData>();
    mesh_geometry->geometry->setRootShape(GeometryBuilder::makeBox(0.0, 0.0, 0.0, 1.0, 1.0, 1.0));
    components.push_back(std::move(mesh_geometry));

    auto geometry_only = std::make_unique<ComponentData>();
    geometry_only->geometry = std::make_unique<GeometryData>();
    geometry_only->geometry->setRootShape(GeometryBuilder::makeBox(2.0, 0.0, 0.0, 1.0, 1.0, 1.0));
    components.push_back(std::move(geometry_only));

    auto mesh_only = std::make_unique<ComponentData>();
    mesh_only->mesh = std::make_unique<MeshData>();
    components.push_back(std::move(mesh_only));

    const Index model_id = model.addModel("toggle fixture", std::move(components));
    const auto component_ids = model.modelById(model_id)->componentIds();
    REQUIRE(component_ids.size() == 3);

    auto* tree = f.tree->findChild<TreeModel*>();
    auto* viewport = f.viewport->findChild<QRenderWindow*>();
    REQUIRE(tree);
    REQUIRE(viewport);
    REQUIRE(tree->refresh());
    for (Index component_id : component_ids)
        viewport->onComponentChanged(component_id);

    std::map<Index, std::pair<bool, bool>> last;
    QObject::connect(viewport, &QRenderWindow::componentVisibilityChanged, viewport,
        [&last](Index component_id, bool mesh_visible, bool geometry_visible) {
            last[component_id] = { mesh_visible, geometry_visible };
        });

    const auto check_state = [&](bool visible) {
        const QModelIndex model_index = tree->findIndexByNodeId(model_id, 0);
        REQUIRE(model_index.isValid());
        CHECK(tree->data(model_index, TreeModel::IsVisibleRole).toBool() == visible);
        CHECK(tree->data(model_index, TreeModel::VisibilityStateRole).toInt()
            == (visible ? Qt::Checked : Qt::Unchecked));
        for (Index component_id : component_ids) {
            const QModelIndex component_index = tree->findIndexByNodeId(component_id, 1);
            REQUIRE(component_index.isValid());
            CHECK(tree->data(component_index, TreeModel::IsVisibleRole).toBool() == visible);
            CHECK(tree->data(component_index, TreeModel::VisibilityStateRole).toInt()
                == (visible ? Qt::Checked : Qt::Unchecked));
            const ComponentData* component = model.findComponent(component_id);
            REQUIRE(component);
            REQUIRE(last.contains(component_id));
            CHECK(last.at(component_id)
                == std::make_pair(static_cast<bool>(component->mesh) && visible,
                                  static_cast<bool>(component->geometry) && visible));
        }
    };

    for (int cycle = 0; cycle < 4; ++cycle) {
        REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "hideAllNodes"));
        pumpEvents();
        check_state(false);
        REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "showAllNodes"));
        pumpEvents();
        check_state(true);
    }
}

//! @brief QML 只验证真实入口的目标范围；隐藏面恢复由视口及数据管线测试覆盖。
TEST_CASE("Actual QML visibility entries dispatch to the intended components", "[QML][visibility]")
{
    application();
    QmlFixture f;
    auto& model = *f.manager->getModelManager();
    model.setUndoRecorder(nullptr);
    const auto add_model = [&](int component_count) {
        ComponentDatas components;
        for (int i = 0; i < component_count; ++i) {
            auto component = std::make_unique<ComponentData>();
            component->mesh = std::make_unique<MeshData>();
            components.push_back(std::move(component));
        }
        return model.addModel("visibility fixture", std::move(components));
    };
    const Index model_id = add_model(2);
    const Index other_model_id = add_model(1);
    const auto component_ids = model.modelById(model_id)->componentIds();
    const Index id = component_ids[0];
    const Index second_id = component_ids[1];
    const Index other_id = model.modelById(other_model_id)->componentIds()[0];
    auto* tree = f.tree->findChild<TreeModel*>();
    auto* viewport = f.viewport->findChild<QRenderWindow*>();
    REQUIRE(tree);
    REQUIRE(viewport);
    REQUIRE(tree->refresh());
    // 只建立 GUI 显隐状态；视口不挂入窗口，因此不会创建 VTK/OpenGL 场景。
    for (Index component_id : { id, second_id, other_id }) {
        auto data = f.manager->getModelQuery()->getMeshDataByComponent(component_id);
        REQUIRE(data);
        viewport->loadMeshData(component_id, *data);
        viewport->setMeshVisibility(component_id, false);
    }
    REQUIRE(viewport->window() == nullptr);
    std::map<Index, std::pair<bool, bool>> notifications;
    QObject::connect(viewport, &QRenderWindow::componentVisibilityChanged, viewport,
                     [&notifications](Index component_id, bool mesh_visible, bool geometry_visible) {
                         notifications[component_id] = { mesh_visible, geometry_visible };
                     });
    QString show_entry;
    bool show_scene = false;
    bool show_model = false;
    SECTION("object tree toolbar")
    {
        show_scene = true;
        REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "showAllNodes"));
    }
    SECTION("viewport context menu")
    {
        show_scene = true;
        auto* menu = f.viewport->findChild<QObject*>("viewportVisibilityMenu");
        REQUIRE(menu);
        QObject* show_all = nullptr;
        for (auto* item : menu->findChildren<QObject*>()) {
            if (item->property("shown").isValid() && item->property("text").toString() == "全部显示")
                show_all = item;
        }
        REQUIRE(show_all);
        REQUIRE(QMetaObject::invokeMethod(show_all, "triggered"));
    }
    SECTION("object tree component show") { show_entry = QString("showNode-1-%1").arg(id); }
    SECTION("object tree mesh show") { show_entry = QString("showNode-2-%1").arg(id); }
    SECTION("object tree model show")
    {
        show_model = true;
        show_entry = QString("showNode-0-%1").arg(model_id);
    }
    if (!show_entry.isEmpty()) {
        // 软件场景图仅布局对象树，保留真实菜单的创建、可见性和触发接线检查。
        QQuickWindow host;
        host.resize(300, 600);
        auto* tree_page = qobject_cast<QQuickItem*>(f.tree.get());
        REQUIRE(tree_page);
        tree_page->setParentItem(host.contentItem());
        tree_page->setSize(QSizeF(300, 600));
        const auto cleanup = qScopeGuard([&] {
            host.hide();
            tree_page->setParentItem(nullptr);
        });
        host.show();
        QCoreApplication::processEvents();
        auto* tree_view = f.tree->findChild<QObject*>("objectTreeView");
        REQUIRE(tree_view);
        REQUIRE(QMetaObject::invokeMethod(tree_view, "expandRecursively", Q_ARG(int, -1), Q_ARG(int, -1)));
        REQUIRE(QMetaObject::invokeMethod(tree_view, "forceLayout"));
        auto node_index = tree->findIndexByNodeId(id, 1);
        if (show_model)
            node_index = node_index.parent();
        else if (show_entry.startsWith("showNode-2-"))
            node_index = tree->index(0, 0, node_index);
        auto* selection_model = tree_view->property("selectionModel").value<QItemSelectionModel*>();
        REQUIRE(selection_model);
        selection_model->setCurrentIndex(node_index, QItemSelectionModel::ClearAndSelect);
        QQuickItem* delegate = nullptr;
        REQUIRE(QMetaObject::invokeMethod(tree_view, "itemAtIndex",
            Q_RETURN_ARG(QQuickItem*, delegate), Q_ARG(QModelIndex, node_index)));
        REQUIRE(delegate);
        auto* show = delegate->findChild<QObject*>(show_entry);
        REQUIRE(show);
        CHECK(show->property("text").toString() == "显示");
        auto* menu = show->property("menu").value<QObject*>();
        REQUIRE(menu);
        REQUIRE(QMetaObject::invokeMethod(menu, "open"));
        QCoreApplication::processEvents();
        CHECK(show->property("visible").toBool());
        CHECK(show->property("height").toDouble() > 0);
        REQUIRE(QMetaObject::invokeMethod(show, "triggered"));
        REQUIRE(QMetaObject::invokeMethod(menu, "close"));
    }
    const auto check_visibility = [&](Index component_id, bool expected) {
        const auto index = tree->findIndexByNodeId(component_id, 1);
        CHECK(tree->data(index, TreeModel::IsVisibleRole).toBool() == expected);
        CHECK(notifications.contains(component_id) == expected);
        if (expected)
            CHECK(notifications.at(component_id) == std::make_pair(true, false));
    };
    check_visibility(id, true);
    check_visibility(second_id, show_scene || show_model);
    check_visibility(other_id, show_scene);
    REQUIRE(viewport->window() == nullptr);
    if (show_scene) {
        notifications.clear();
        REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "reverseDisplayed"));
        REQUIRE(notifications.size() == 3);
        for (const auto& [component_id, visibility] : notifications) {
            CHECK(visibility == std::make_pair(false, false));
            CHECK_FALSE(tree->data(tree->findIndexByNodeId(component_id, 1), TreeModel::IsVisibleRole).toBool());
        }
    }
}

TEST_CASE("Actual QML aligns face and component visibility menu entries", "[QML][visibility]")
{
    application();
    QmlFixture f;
    auto* menu = f.viewport->findChild<QObject*>("viewportVisibilityMenu");
    REQUIRE(menu);
    std::map<QString, QObject*> items;
    for (auto* item : menu->findChildren<QObject*>()) {
        if (item->property("shown").isValid()) {
            const auto text = item->property("text").toString();
            REQUIRE(items.count(text) == 0);
            items[text] = item;
        }
    }
    REQUIRE(items.size() == 6);
    for (const auto& mode : { QString("Face"), QString("Component") }) {
        f.selection_state->setProperty("selectMode", mode);
        for (bool has_picks : { false, true }) {
            // 传入真实 QSelection，覆盖 C++ 选择快照到 QML 菜单的判断路径。
            auto data = std::make_unique<Selection>();
            data->type = mode == "Face" ? ElementEnum::Face : ElementEnum::Component;
            data->component_id = 7;
            if (has_picks)
                data->ids = { 0, 1 };
            QSelection selection(std::move(data));
            REQUIRE(QMetaObject::invokeMethod(menu, "refreshSelection",
                Q_ARG(QVariant, QVariant::fromValue(&selection))));
            CHECK(menu->property("hasPicks").toBool() == has_picks);
            for (const auto& text : { QString("隐藏"), QString("显示"), QString("隔离") })
                CHECK(items.at(text)->property("shown").toBool() == has_picks);
            for (const auto& text : { QString("全部隐藏"), QString("全部显示") })
                CHECK(items.at(text)->property("shown").toBool() == !has_picks);
            CHECK(items.at("反转显示")->property("shown").toBool());
        }
    }
    // 尚未取得选择快照时也应恢复全局操作，不能保留上次菜单的选择状态。
    f.selection_state->setProperty("selectMode", "Face");
    REQUIRE(QMetaObject::invokeMethod(menu, "refreshSelection",
        Q_ARG(QVariant, QVariant::fromValue(static_cast<QObject*>(nullptr)))));
    CHECK_FALSE(menu->property("hasPicks").toBool());
    for (const auto& text : { QString("隐藏"), QString("显示"), QString("隔离") })
        CHECK_FALSE(items.at(text)->property("shown").toBool());
}

TEST_CASE("Actual QML follows successful deletion notifications and preserves selection on refusal", "[QML][undo]")
{
    application();
    QmlFixture f;
    auto* manager = f.manager;
    auto* selection_state = f.selection_state;
    auto& tree = f.tree;
    auto& model = *manager->getModelManager();
    auto* tree_model = tree->findChild<TreeModel*>();
    REQUIRE(tree_model);
    model.setUndoRecorder(nullptr);
    ComponentDatas components;
    components.push_back(std::make_unique<ComponentData>());
    components.push_back(std::make_unique<ComponentData>());
    components.push_back(std::make_unique<ComponentData>());
    const Index mid = model.addModel("fixture", std::move(components));
    const Index cid = model.modelById(mid)->componentIds()[0];
    const Index remaining_cid = model.modelById(mid)->componentIds()[1];
    const Index other_cid = model.modelById(mid)->componentIds()[2];
    selection_state->setProperty("activeModelId", mid);
    selection_state->setProperty("activeComponentId", cid);
    auto operation = model.beginWriteOperation(false);
    for (int depth = 0; depth < 2; ++depth) {
        REQUIRE(QMetaObject::invokeMethod(tree.get(), "deleteNode", Q_ARG(QVariant, depth == 0 ? mid : cid),
            Q_ARG(QVariant, depth), Q_ARG(QVariant, 0), Q_ARG(QVariant, cid)));
        CHECK(selection_state->property("activeModelId").toInt() == mid);
        CHECK(selection_state->property("activeComponentId").toInt() == cid);
        CHECK(model.findComponent(cid));
    }
    operation.reset();
    // 删除非当前项不改变选择。
    model.removeComponent(other_cid);
    CHECK(selection_state->property("activeModelId").toInt() == mid);
    CHECK(selection_state->property("activeComponentId").toInt() == cid);
    const Index other_mid = model.addModel("other", { });
    model.removeModel(other_mid);
    CHECK(selection_state->property("activeModelId").toInt() == mid);
    CHECK(selection_state->property("activeComponentId").toInt() == cid);
    // 直接修改模型层，验证界面响应实际删除通知，独立于对象树命令入口。
    model.removeComponent(cid);
    // 删除当前选中组件后重置选中，不自动选中剩余组件。
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);
    pumpEvents();
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);
    // 后续删除不改变已清空的选中。
    model.removeComponent(remaining_cid);
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);
    pumpEvents();
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);
    model.removeModel(mid);
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);
    pumpEvents();
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);
    // 整体删除带组件的当前模型同样清空选择。
    ComponentDatas model_components;
    model_components.push_back(std::make_unique<ComponentData>());
    model_components.push_back(std::make_unique<ComponentData>());
    const Index selected_mid = model.addModel("selected", std::move(model_components));
    selection_state->setProperty("activeModelId", selected_mid);
    selection_state->setProperty("activeComponentId", model.modelById(selected_mid)->componentIds()[0]);
    model.removeModel(selected_mid);
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);
    pumpEvents();
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);
}

TEST_CASE("Actual QML separates selection from make-current", "[QML][tree][selection]")
{
    application();
    QmlFixture f;
    auto& model = *f.manager->getModelManager();
    model.setUndoRecorder(nullptr);
    auto* tree_model = f.tree->findChild<TreeModel*>();
    auto* tree_view_object = f.tree->findChild<QObject*>("objectTreeView");
    auto* selection_state = f.selection_state;
    REQUIRE(tree_model);
    REQUIRE(tree_view_object);
    REQUIRE(selection_state);

    ComponentDatas components;
    auto first = std::make_unique<ComponentData>();
    first->mesh = std::make_unique<MeshData>();
    components.push_back(std::move(first));
    components.push_back(std::make_unique<ComponentData>());
    const Index model_id = model.addModel("selection fixture", std::move(components));
    const auto component_ids = model.modelById(model_id)->componentIds();
    REQUIRE(component_ids.size() == 2);
    REQUIRE(tree_model->refresh());

    auto* selection_model = tree_view_object->property("selectionModel").value<QItemSelectionModel*>();
    REQUIRE(selection_model);

    QQuickWindow host;
    host.resize(300, 600);
    auto* tree_page = qobject_cast<QQuickItem*>(f.tree.get());
    REQUIRE(tree_page);
    tree_page->setParentItem(host.contentItem());
    tree_page->setSize(QSizeF(300, 600));
    const auto cleanup = qScopeGuard([&] {
        host.hide();
        tree_page->setParentItem(nullptr);
    });
    host.show();
    QCoreApplication::processEvents();
    REQUIRE(QMetaObject::invokeMethod(tree_view_object, "expandRecursively",
        Q_ARG(int, -1), Q_ARG(int, -1)));
    REQUIRE(QMetaObject::invokeMethod(tree_view_object, "forceLayout"));
    QCoreApplication::processEvents();

    const QModelIndex first_index = tree_model->findIndexByNodeId(component_ids[0], 1);
    const QModelIndex second_index = tree_model->findIndexByNodeId(component_ids[1], 1);
    REQUIRE(first_index.isValid());
    REQUIRE(second_index.isValid());
    QQuickItem* first_delegate = nullptr;
    QQuickItem* second_delegate = nullptr;
    REQUIRE(QMetaObject::invokeMethod(tree_view_object, "itemAtIndex",
        Q_RETURN_ARG(QQuickItem*, first_delegate), Q_ARG(QModelIndex, first_index)));
    REQUIRE(QMetaObject::invokeMethod(tree_view_object, "itemAtIndex",
        Q_RETURN_ARG(QQuickItem*, second_delegate), Q_ARG(QModelIndex, second_index)));
    REQUIRE(first_delegate);
    REQUIRE(second_delegate);

    const auto click = [&](QQuickItem* target, Qt::MouseButton button, Qt::KeyboardModifiers modifiers) {
        const QPointF pos = host.contentItem()->mapFromItem(target,
            target->width() / 2, target->height() / 2);
        QMouseEvent press(QEvent::MouseButtonPress, pos, host.mapToGlobal(pos.toPoint()),
            button, button, modifiers);
        QCoreApplication::sendEvent(&host, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, pos, host.mapToGlobal(pos.toPoint()),
            button, Qt::NoButton, modifiers);
        QCoreApplication::sendEvent(&host, &release);
        QCoreApplication::processEvents();
    };
    const auto find_menu_item = [](QQuickItem* delegate, const QString& text) -> QObject* {
        for (auto* child : delegate->findChildren<QObject*>()) {
            if (child->property("text").toString() == text
                && child->property("menu").value<QObject*>())
                return child;
        }
        return nullptr;
    };
    const auto selection_count = [&] {
        return selection_model->selectedIndexes().size();
    };

    // 普通点击只改选中，不切换活动对象。
    click(first_delegate, Qt::LeftButton, Qt::NoModifier);
    CHECK(selection_count() == 1);
    CHECK(selection_model->currentIndex() == first_index);
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);

    // 再次点击同一行取消选中；再点击恢复。
    click(first_delegate, Qt::LeftButton, Qt::NoModifier);
    CHECK(selection_count() == 0);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);
    click(first_delegate, Qt::LeftButton, Qt::NoModifier);
    CHECK(selection_count() == 1);

    // 再次普通点击另一行替换选中，活动对象仍不变。
    click(second_delegate, Qt::LeftButton, Qt::NoModifier);
    CHECK(selection_count() == 1);
    CHECK_FALSE(selection_model->isSelected(first_index));
    CHECK(selection_model->isSelected(second_index));
    CHECK(selection_model->currentIndex() == second_index);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);

    // 右键选中行打开菜单；单目标命令保持可用，设置活动组件只切换活动对象。
    click(second_delegate, Qt::RightButton, Qt::NoModifier);
    auto* make_current = second_delegate->findChild<QObject*>("activeComponentItem");
    REQUIRE(make_current);
    CHECK(make_current->property("text").toString() == "设置活动组件");
    auto* menu = make_current->property("menu").value<QObject*>();
    REQUIRE(menu);
    CHECK(menu->property("visible").toBool());
    auto* delete_item = find_menu_item(second_delegate, "删除");
    REQUIRE(delete_item);
    CHECK(delete_item->property("enabled").toBool());
    REQUIRE(QMetaObject::invokeMethod(make_current, "triggered"));
    QCoreApplication::processEvents();
    CHECK(selection_state->property("activeModelId").toInt() == model_id);
    CHECK(selection_state->property("activeComponentId").toInt() == component_ids[1]);
    CHECK(selection_count() == 1);

    // 再选中其他行不会改变活动对象与加粗目标。
    click(first_delegate, Qt::LeftButton, Qt::NoModifier);
    CHECK(selection_count() == 1);
    CHECK(selection_state->property("activeComponentId").toInt() == component_ids[1]);
    QVariant emphasized;
    REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "isNodeEmphasized",
        Q_RETURN_ARG(QVariant, emphasized),
        Q_ARG(QVariant, component_ids[1]), Q_ARG(QVariant, 1)));
    CHECK(emphasized.toBool());

    // undo/redo 应用后清空行选择；仍有效的活动对象保留。
    auto* undo = f.manager->getUndoStackAdaptor();
    REQUIRE(undo);
    REQUIRE(QMetaObject::invokeMethod(undo, "applied"));
    CHECK_FALSE(selection_model->hasSelection());
    CHECK(selection_state->property("activeComponentId").toInt() == component_ids[1]);

    // 重新左键选中当前活动 Component，右键菜单项切换为取消。
    click(second_delegate, Qt::LeftButton, Qt::NoModifier);
    click(second_delegate, Qt::RightButton, Qt::NoModifier);
    auto* cancel_current = second_delegate->findChild<QObject*>("activeComponentItem");
    REQUIRE(cancel_current);
    CHECK(cancel_current->property("text").toString() == "取消活动组件");
    REQUIRE(QMetaObject::invokeMethod(cancel_current, "triggered"));
    QCoreApplication::processEvents();
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);
    CHECK(selection_count() == 1);

    // depth=2 的 Mesh 使用独立菜单：不出现设置/取消活动组件。
    const QModelIndex mesh_index = tree_model->index(0, 0, first_index);
    REQUIRE(mesh_index.isValid());
    QQuickItem* mesh_delegate = nullptr;
    REQUIRE(QMetaObject::invokeMethod(tree_view_object, "itemAtIndex",
        Q_RETURN_ARG(QQuickItem*, mesh_delegate), Q_ARG(QModelIndex, mesh_index)));
    REQUIRE(mesh_delegate);
    click(mesh_delegate, Qt::LeftButton, Qt::NoModifier);
    click(mesh_delegate, Qt::RightButton, Qt::NoModifier);
    auto* mesh_menu = mesh_delegate->findChild<QObject*>("nodeActionsMenu");
    REQUIRE(mesh_menu);
    CHECK(mesh_menu->property("visible").toBool());
    bool mesh_has_active_items = false;
    for (auto* child : mesh_menu->findChildren<QObject*>()) {
        const QString text = child->property("text").toString();
        if (text == "设置活动组件" || text == "取消活动组件")
            mesh_has_active_items = true;
    }
    CHECK_FALSE(mesh_has_active_items);
    REQUIRE(QMetaObject::invokeMethod(mesh_menu, "close"));
}

TEST_CASE("Actual QML enforces set and cancel active component rules", "[QML][tree][selection]")
{
    application();
    QmlFixture f;
    auto& model = *f.manager->getModelManager();
    model.setUndoRecorder(nullptr);
    auto* tree_model = f.tree->findChild<TreeModel*>();
    auto* tree_view_object = f.tree->findChild<QObject*>("objectTreeView");
    auto* selection_state = f.selection_state;
    REQUIRE(tree_model);
    REQUIRE(tree_view_object);
    REQUIRE(selection_state);
    auto* selection_model = tree_view_object->property("selectionModel").value<QItemSelectionModel*>();
    REQUIRE(selection_model);

    ComponentDatas components;
    components.push_back(std::make_unique<ComponentData>());
    components.push_back(std::make_unique<ComponentData>());
    const Index model_id = model.addModel("active fixture", std::move(components));
    const auto component_ids = model.modelById(model_id)->componentIds();
    REQUIRE(component_ids.size() == 2);
    REQUIRE(tree_model->refresh());

    QQuickWindow host;
    host.resize(400, 600);
    auto* tree_page = qobject_cast<QQuickItem*>(f.tree.get());
    REQUIRE(tree_page);
    tree_page->setParentItem(host.contentItem());
    tree_page->setSize(QSizeF(300, 600));
    const auto cleanup = qScopeGuard([&] {
        host.hide();
        tree_page->setParentItem(nullptr);
    });
    host.show();
    QCoreApplication::processEvents();
    REQUIRE(QMetaObject::invokeMethod(tree_view_object, "expandRecursively",
        Q_ARG(int, -1), Q_ARG(int, -1)));
    REQUIRE(QMetaObject::invokeMethod(tree_view_object, "forceLayout"));
    QCoreApplication::processEvents();

    const QModelIndex first_index = tree_model->findIndexByNodeId(component_ids[0], 1);
    const QModelIndex second_index = tree_model->findIndexByNodeId(component_ids[1], 1);
    REQUIRE(first_index.isValid());
    REQUIRE(second_index.isValid());
    QQuickItem* first_delegate = nullptr;
    QQuickItem* second_delegate = nullptr;
    REQUIRE(QMetaObject::invokeMethod(tree_view_object, "itemAtIndex",
        Q_RETURN_ARG(QQuickItem*, first_delegate), Q_ARG(QModelIndex, first_index)));
    REQUIRE(QMetaObject::invokeMethod(tree_view_object, "itemAtIndex",
        Q_RETURN_ARG(QQuickItem*, second_delegate), Q_ARG(QModelIndex, second_index)));
    REQUIRE(first_delegate);
    REQUIRE(second_delegate);

    const auto click = [&](QQuickItem* target, Qt::MouseButton button) {
        const QPointF pos = host.contentItem()->mapFromItem(target,
            target->width() / 2, target->height() / 2);
        QMouseEvent press(QEvent::MouseButtonPress, pos, host.mapToGlobal(pos.toPoint()),
            button, button, Qt::NoModifier);
        QCoreApplication::sendEvent(&host, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, pos, host.mapToGlobal(pos.toPoint()),
            button, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&host, &release);
        QCoreApplication::processEvents();
    };
    const auto active_item = [](QQuickItem* delegate) -> QObject* {
        return delegate->findChild<QObject*>("activeComponentItem");
    };
    const auto open_menu = [&](QQuickItem* delegate, const QModelIndex& index) -> QObject* {
        if (!selection_model->isSelected(index))
            selection_model->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
        click(delegate, Qt::RightButton);
        auto* item = active_item(delegate);
        REQUIRE(item);
        auto* menu = item->property("menu").value<QObject*>();
        REQUIRE(menu);
        REQUIRE(menu->property("visible").toBool());
        return menu;
    };
    const auto close_menu = [](QObject* menu) {
        REQUIRE(QMetaObject::invokeMethod(menu, "close"));
        QCoreApplication::processEvents();
    };

    // 右键未选中行不改变选中，只打开全局菜单。
    click(first_delegate, Qt::RightButton);
    CHECK_FALSE(selection_model->hasSelection());
    auto* global_menu = f.tree->findChild<QObject*>("blankContextMenu");
    REQUIRE(global_menu);
    CHECK(global_menu->property("visible").toBool());
    close_menu(global_menu);

    // 未设置活动组件：菜单项为设置。
    auto* menu = open_menu(first_delegate, first_index);
    auto* item = active_item(first_delegate);
    REQUIRE(item);
    CHECK(item->property("text").toString() == "设置活动组件");
    REQUIRE(QMetaObject::invokeMethod(item, "triggered"));
    QCoreApplication::processEvents();
    CHECK(selection_state->property("activeComponentId").toInt() == component_ids[0]);

    // 当前活动行：菜单项为取消。
    menu = open_menu(first_delegate, first_index);
    item = active_item(first_delegate);
    REQUIRE(item);
    CHECK(item->property("text").toString() == "取消活动组件");
    close_menu(menu);

    // 其他行：菜单项为设置，触发后直接切换活动组件。
    menu = open_menu(second_delegate, second_index);
    item = active_item(second_delegate);
    REQUIRE(item);
    CHECK(item->property("text").toString() == "设置活动组件");
    REQUIRE(QMetaObject::invokeMethod(item, "triggered"));
    QCoreApplication::processEvents();
    CHECK(selection_state->property("activeComponentId").toInt() == component_ids[1]);

    // 当前活动行取消后，恢复为设置文案。
    menu = open_menu(second_delegate, second_index);
    item = active_item(second_delegate);
    REQUIRE(item);
    CHECK(item->property("text").toString() == "取消活动组件");
    REQUIRE(QMetaObject::invokeMethod(item, "triggered"));
    QCoreApplication::processEvents();
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);

    menu = open_menu(first_delegate, first_index);
    item = active_item(first_delegate);
    REQUIRE(item);
    CHECK(item->property("text").toString() == "设置活动组件");
    close_menu(menu);
}

TEST_CASE("Actual QML clears selection on outside left press", "[QML][tree][selection]")
{
    application();
    QmlFixture f;
    auto& model = *f.manager->getModelManager();
    model.setUndoRecorder(nullptr);
    auto* tree_model = f.tree->findChild<TreeModel*>();
    auto* tree_view_object = f.tree->findChild<QObject*>("objectTreeView");
    auto* selection_state = f.selection_state;
    REQUIRE(tree_model);
    REQUIRE(tree_view_object);
    REQUIRE(selection_state);

    ComponentDatas components;
    components.push_back(std::make_unique<ComponentData>());
    const Index model_id = model.addModel("outside click fixture", std::move(components));
    const Index component_id = model.modelById(model_id)->componentIds().front();
    REQUIRE(tree_model->refresh());

    QQuickWindow host;
    host.resize(400, 600);
    auto* tree_page = qobject_cast<QQuickItem*>(f.tree.get());
    REQUIRE(tree_page);
    tree_page->setParentItem(host.contentItem());
    tree_page->setSize(QSizeF(300, 600));
    const auto cleanup = qScopeGuard([&] {
        host.hide();
        tree_page->setParentItem(nullptr);
    });
    host.show();
    QCoreApplication::processEvents();
    REQUIRE(QMetaObject::invokeMethod(tree_view_object, "expandRecursively",
        Q_ARG(int, -1), Q_ARG(int, -1)));
    REQUIRE(QMetaObject::invokeMethod(tree_view_object, "forceLayout"));
    QCoreApplication::processEvents();

    const QModelIndex component_index = tree_model->findIndexByNodeId(component_id, 1);
    REQUIRE(component_index.isValid());
    QQuickItem* delegate = nullptr;
    REQUIRE(QMetaObject::invokeMethod(tree_view_object, "itemAtIndex",
        Q_RETURN_ARG(QQuickItem*, delegate), Q_ARG(QModelIndex, component_index)));
    REQUIRE(delegate);
    auto* selection_model = tree_view_object->property("selectionModel").value<QItemSelectionModel*>();
    REQUIRE(selection_model);
    const QPointF inside_local = host.contentItem()->mapFromItem(delegate,
        delegate->width() / 2, delegate->height() / 2);
    const QPointF inside_global = host.mapToGlobal(inside_local.toPoint());
    const QPointF outside_global = host.mapToGlobal(QPoint(380, 500));
    const auto click = [&](Qt::MouseButton button) {
        QMouseEvent press(QEvent::MouseButtonPress, inside_local,
            host.mapToGlobal(inside_local.toPoint()), button, button, Qt::NoModifier);
        QCoreApplication::sendEvent(&host, &press);
        QMouseEvent release(QEvent::MouseButtonRelease, inside_local,
            host.mapToGlobal(inside_local.toPoint()), button, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&host, &release);
        QCoreApplication::processEvents();
    };
    const auto press_outside = [&] {
        REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "handleGlobalLeftPress",
            Q_ARG(QVariant, outside_global.x()), Q_ARG(QVariant, outside_global.y())));
    };
    const auto press_inside = [&] {
        REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "handleGlobalLeftPress",
            Q_ARG(QVariant, inside_global.x()), Q_ARG(QVariant, inside_global.y())));
    };

    click(Qt::LeftButton);
    REQUIRE(selection_model->selectedIndexes().size() == 1);
    REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "makeNodeCurrent",
        Q_ARG(QVariant, component_id), Q_ARG(QVariant, 1), Q_ARG(QVariant, component_id)));
    CHECK(selection_state->property("activeComponentId").toInt() == component_id);

    // 点击对象树外部：清空行选择，保留活动组件。
    press_outside();
    CHECK_FALSE(selection_model->hasSelection());
    CHECK(selection_state->property("activeModelId").toInt() == model_id);
    CHECK(selection_state->property("activeComponentId").toInt() == component_id);

    // 点击对象树内部：保持选择。
    click(Qt::LeftButton);
    REQUIRE(selection_model->selectedIndexes().size() == 1);
    press_inside();
    CHECK(selection_model->selectedIndexes().size() == 1);

    // 自身菜单打开时，外部按下先只关闭菜单，不清选择。
    click(Qt::RightButton);
    pumpEvents();
    REQUIRE(selection_model->selectedIndexes().size() == 1);
    press_outside();
    CHECK(selection_model->selectedIndexes().size() == 1);
    auto* active_item = delegate->findChild<QObject*>("activeComponentItem");
    REQUIRE(active_item);
    auto* menu = active_item->property("menu").value<QObject*>();
    REQUIRE(menu);
    REQUIRE(QMetaObject::invokeMethod(menu, "close"));
    pumpEvents();
}

TEST_CASE("Actual QML shows unavailable menu for deep rows", "[QML][tree][selection]")
{
    application();
    QmlFixture f;
    auto& model = *f.manager->getModelManager();
    model.setUndoRecorder(nullptr);
    auto* tree_model = f.tree->findChild<TreeModel*>();
    auto* tree_view_object = f.tree->findChild<QObject*>("objectTreeView");
    auto* selection_state = f.selection_state;
    REQUIRE(tree_model);
    REQUIRE(tree_view_object);
    REQUIRE(selection_state);

    ComponentDatas components;
    auto component = std::make_unique<ComponentData>();
    component->mesh = std::make_unique<MeshData>();
    component->mesh->solid_types_.assign(1, 0);
    components.push_back(std::move(component));
    const Index model_id = model.addModel("deep menu fixture", std::move(components));
    const Index component_id = model.modelById(model_id)->componentIds().front();
    REQUIRE(tree_model->refresh());

    QQuickWindow host;
    host.resize(300, 600);
    auto* tree_page = qobject_cast<QQuickItem*>(f.tree.get());
    REQUIRE(tree_page);
    tree_page->setParentItem(host.contentItem());
    tree_page->setSize(QSizeF(300, 600));
    const auto cleanup = qScopeGuard([&] {
        host.hide();
        tree_page->setParentItem(nullptr);
    });
    host.show();
    QCoreApplication::processEvents();
    REQUIRE(QMetaObject::invokeMethod(tree_view_object, "expandRecursively",
        Q_ARG(int, -1), Q_ARG(int, -1)));
    REQUIRE(QMetaObject::invokeMethod(tree_view_object, "forceLayout"));
    QCoreApplication::processEvents();

    const QModelIndex component_index = tree_model->findIndexByNodeId(component_id, 1);
    REQUIRE(component_index.isValid());
    const QModelIndex mesh_index = tree_model->index(0, 0, component_index);
    REQUIRE(mesh_index.isValid());
    const QModelIndex sub_index = tree_model->index(0, 0, mesh_index);
    REQUIRE(sub_index.isValid());
    QQuickItem* delegate = nullptr;
    REQUIRE(QMetaObject::invokeMethod(tree_view_object, "itemAtIndex",
        Q_RETURN_ARG(QQuickItem*, delegate), Q_ARG(QModelIndex, sub_index)));
    REQUIRE(delegate);
    auto* selection_model = tree_view_object->property("selectionModel").value<QItemSelectionModel*>();
    REQUIRE(selection_model);

    // 左键选中深层行后，右键打开深层菜单。
    const QPointF pos = host.contentItem()->mapFromItem(delegate,
        delegate->width() / 2, delegate->height() / 2);
    QMouseEvent left_press(QEvent::MouseButtonPress, pos, host.mapToGlobal(pos.toPoint()),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&host, &left_press);
    QMouseEvent left_release(QEvent::MouseButtonRelease, pos, host.mapToGlobal(pos.toPoint()),
        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&host, &left_release);
    QCoreApplication::processEvents();
    CHECK(selection_model->isSelected(sub_index));
    QMouseEvent press(QEvent::MouseButtonPress, pos, host.mapToGlobal(pos.toPoint()),
        Qt::RightButton, Qt::RightButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&host, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, host.mapToGlobal(pos.toPoint()),
        Qt::RightButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&host, &release);
    QCoreApplication::processEvents();
    CHECK(selection_state->property("activeModelId").toInt() == -1);

    auto* menu = delegate->findChild<QObject*>("deepContextMenu");
    REQUIRE(menu);
    CHECK(menu->property("visible").toBool());
    bool has_unavailable = false;
    bool has_active_item = false;
    for (auto* child : menu->findChildren<QObject*>()) {
        const QString text = child->property("text").toString();
        if (text == "功能开发中") {
            has_unavailable = true;
            CHECK_FALSE(child->property("enabled").toBool());
        } else if (text == "设置活动组件" || text == "取消活动组件") {
            has_active_item = true;
        }
    }
    CHECK(has_unavailable);
    CHECK_FALSE(has_active_item);
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);
    CHECK(selection_model->isSelected(sub_index));
    REQUIRE(QMetaObject::invokeMethod(menu, "close"));
}

TEST_CASE("Actual QML navigates object tree rows with keyboard", "[QML][tree][selection]")
{
    application();
    QmlFixture f;
    auto& model = *f.manager->getModelManager();
    model.setUndoRecorder(nullptr);
    auto* tree_model = f.tree->findChild<TreeModel*>();
    auto* tree_view_object = f.tree->findChild<QObject*>("objectTreeView");
    REQUIRE(tree_model);
    REQUIRE(tree_view_object);
    auto* selection_model = tree_view_object->property("selectionModel").value<QItemSelectionModel*>();
    auto* tree_view = qobject_cast<QQuickItem*>(tree_view_object);
    REQUIRE(selection_model);
    REQUIRE(tree_view);

    ComponentDatas components;
    components.push_back(std::make_unique<ComponentData>());
    components.push_back(std::make_unique<ComponentData>());
    const Index model_id = model.addModel("keyboard fixture", std::move(components));
    const auto component_ids = model.modelById(model_id)->componentIds();
    REQUIRE(tree_model->refresh());

    QQuickWindow host;
    host.resize(300, 600);
    auto* tree_page = qobject_cast<QQuickItem*>(f.tree.get());
    REQUIRE(tree_page);
    tree_page->setParentItem(host.contentItem());
    tree_page->setSize(QSizeF(300, 600));
    const auto cleanup = qScopeGuard([&] {
        host.hide();
        tree_page->setParentItem(nullptr);
    });
    host.show();
    QCoreApplication::processEvents();
    REQUIRE(QMetaObject::invokeMethod(tree_view_object, "expandRecursively",
        Q_ARG(int, -1), Q_ARG(int, -1)));
    REQUIRE(QMetaObject::invokeMethod(tree_view_object, "forceLayout"));
    QCoreApplication::processEvents();

    const QModelIndex first_index = tree_model->findIndexByNodeId(component_ids[0], 1);
    const QModelIndex second_index = tree_model->findIndexByNodeId(component_ids[1], 1);
    REQUIRE(first_index.isValid());
    REQUIRE(second_index.isValid());
    tree_view->forceActiveFocus();
    const auto select_first = [&] {
        REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "selectRow",
            Q_ARG(QVariant, QVariant::fromValue(first_index)),
            Q_ARG(QVariant, false)));
    };
    const auto send_key = [&](int key, Qt::KeyboardModifiers modifiers) {
        QKeyEvent press(QEvent::KeyPress, key, modifiers);
        QCoreApplication::sendEvent(&host, &press);
        QKeyEvent release(QEvent::KeyRelease, key, modifiers);
        QCoreApplication::sendEvent(&host, &release);
        QCoreApplication::processEvents();
    };

    // 普通方向键：移动并替换选中。
    select_first();
    send_key(Qt::Key_Down, Qt::NoModifier);
    CHECK(selection_model->selectedIndexes().size() == 1);
    CHECK(selection_model->isSelected(second_index));
    CHECK(selection_model->currentIndex() == second_index);

    // 带修饰键的方向键仍按单选处理，不产生多选。
    select_first();
    send_key(Qt::Key_Down, Qt::ControlModifier);
    CHECK(selection_model->selectedIndexes().size() == 1);
    CHECK(selection_model->isSelected(second_index));
    CHECK(selection_model->currentIndex() == second_index);

    select_first();
    send_key(Qt::Key_Down, Qt::ShiftModifier);
    CHECK(selection_model->selectedIndexes().size() == 1);
    CHECK(selection_model->isSelected(second_index));
    CHECK(selection_model->currentIndex() == second_index);
}

TEST_CASE("Actual QML emphasizes only model and component rows", "[QML][tree][selection]")
{
    application();
    QmlFixture f;
    auto& model = *f.manager->getModelManager();
    model.setUndoRecorder(nullptr);
    auto* tree_model = f.tree->findChild<TreeModel*>();
    REQUIRE(tree_model);

    ComponentDatas components;
    auto component = std::make_unique<ComponentData>();
    component->mesh = std::make_unique<MeshData>();
    components.push_back(std::move(component));
    const Index model_id = model.addModel("emphasis fixture", std::move(components));
    const Index component_id = model.modelById(model_id)->componentIds().front();
    REQUIRE(tree_model->refresh());

    const QModelIndex component_index = tree_model->findIndexByNodeId(component_id, 1);
    REQUIRE(component_index.isValid());
    const QModelIndex mesh_index = tree_model->index(0, 0, component_index);
    REQUIRE(mesh_index.isValid());
    const Index mesh_node_id = tree_model->data(mesh_index, TreeModel::NodeIdRole).toInt();

    const auto make_current = [&](Index node_id, int depth, Index owner_id) {
        REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "makeNodeCurrent",
            Q_ARG(QVariant, node_id), Q_ARG(QVariant, depth), Q_ARG(QVariant, owner_id)));
    };
    const auto emphasized = [&](Index node_id, int depth) {
        QVariant result;
        REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "isNodeEmphasized",
            Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, node_id), Q_ARG(QVariant, depth)));
        return result.toBool();
    };

    // depth>1 不能设置活动组件，调用不改变活动目标。
    make_current(mesh_node_id, 2, component_id);
    CHECK_FALSE(emphasized(model_id, 0));
    CHECK_FALSE(emphasized(component_id, 1));
    CHECK_FALSE(emphasized(mesh_node_id, 2));

    // 设置活动组件到 Model：Model 与其首个 Component 加粗。
    make_current(model_id, 0, -1);
    CHECK(emphasized(model_id, 0));
    CHECK(emphasized(component_id, 1));
    CHECK_FALSE(emphasized(mesh_node_id, 2));

    model.removeModel(model_id);
    pumpEvents();
}

TEST_CASE("Actual QML deletes the selected object tree node", "[QML][tree][selection][delete]")
{
    application();
    QmlFixture f;
    auto& model = *f.manager->getModelManager();
    model.setUndoRecorder(nullptr);
    auto* tree_model = f.tree->findChild<TreeModel*>();
    auto* tree_view_object = f.tree->findChild<QObject*>("objectTreeView");
    auto* selection_state = f.selection_state;
    REQUIRE(tree_model);
    REQUIRE(tree_view_object);
    REQUIRE(selection_state);
    auto* selection_model = tree_view_object->property("selectionModel").value<QItemSelectionModel*>();
    REQUIRE(selection_model);

    ComponentDatas components;
    auto first = std::make_unique<ComponentData>();
    first->mesh = std::make_unique<MeshData>();
    components.push_back(std::move(first));
    components.push_back(std::make_unique<ComponentData>());
    const Index model_id = model.addModel("delete fixture", std::move(components));
    const auto component_ids = model.modelById(model_id)->componentIds();
    const Index component_id = component_ids[0];
    REQUIRE(tree_model->refresh());

    const auto select_index = [&](const QModelIndex& index,
                                  QItemSelectionModel::SelectionFlags flags
                                  = QItemSelectionModel::ClearAndSelect) {
        selection_model->setCurrentIndex(index, flags);
    };
    const auto delete_selected = [&] {
        QVariant deleted;
        REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "deleteSelectedNode",
            Q_RETURN_ARG(QVariant, deleted)));
        return deleted.toBool();
    };

    // 无选中时不执行删除。
    CHECK_FALSE(delete_selected());
    const QModelIndex first_component_index = tree_model->findIndexByNodeId(component_ids[0], 1);
    REQUIRE(first_component_index.isValid());
    CHECK(model.findComponent(component_ids[0]));
    CHECK(model.findComponent(component_ids[1]));

    // make current 后删除 Mesh：组件保留，活动对象保留。
    REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "makeNodeCurrent",
        Q_ARG(QVariant, component_id), Q_ARG(QVariant, 1), Q_ARG(QVariant, component_id)));
    const QModelIndex mesh_index = tree_model->index(0, 0, first_component_index);
    REQUIRE(mesh_index.isValid());
    const Index mesh_node_id = tree_model->data(mesh_index, TreeModel::NodeIdRole).toInt();
    select_index(mesh_index);
    CHECK(delete_selected());
    CHECK(model.findComponent(component_id));
    CHECK(model.findComponent(component_id)->mesh == nullptr);
    CHECK(selection_state->property("activeModelId").toInt() == model_id);
    CHECK(selection_state->property("activeComponentId").toInt() == component_id);
    // 等待树结构移除 Mesh 行后，行选择随之失效，重复 Delete 不会删除父组件。
    REQUIRE(waitForCondition([&] {
        return !tree_model->findIndexByNodeId(mesh_node_id, 2).isValid();
    }));
    CHECK_FALSE(delete_selected());
    CHECK(model.findComponent(component_id));

    // 删除活动 Component：所属模型保留，活动对象清理。
    select_index(tree_model->findIndexByNodeId(component_id, 1));
    CHECK(delete_selected());
    CHECK(model.findComponent(component_id) == nullptr);
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);

    // 删除 Model：模型移除。
    select_index(tree_model->findIndexByNodeId(model_id, 0));
    CHECK(delete_selected());
    CHECK(model.modelById(model_id) == nullptr);
    CHECK(selection_state->property("activeModelId").toInt() == -1);
    CHECK(selection_state->property("activeComponentId").toInt() == -1);

    pumpEvents();
}

TEST_CASE("Actual QML preserves expanded rows across incremental imports and deletes", "[QML][tree]")
{
    application();
    QmlFixture f;
    auto& model = *f.manager->getModelManager();
    model.setUndoRecorder(nullptr);
    auto* tree_model = f.tree->findChild<TreeModel*>();
    auto* tree_view = f.tree->findChild<QObject*>("objectTreeView");
    REQUIRE(tree_model);
    REQUIRE(tree_view);

    auto make_components = [] {
        ComponentDatas components;
        auto component = std::make_unique<ComponentData>();
        component->mesh = std::make_unique<MeshData>();
        components.push_back(std::move(component));
        return components;
    };

    const Index first_model = model.addModel("first", make_components());
    const Index second_model = model.addModel("second", make_components());
    REQUIRE(tree_model->refresh());
    REQUIRE(QMetaObject::invokeMethod(tree_view, "expandRecursively",
        Q_ARG(int, -1), Q_ARG(int, -1)));
    REQUIRE(QMetaObject::invokeMethod(tree_view, "forceLayout"));
    QCoreApplication::processEvents();

    const Index target_component = model.modelById(second_model)->componentIds().front();
    auto row_of = [&](const QModelIndex& index) {
        int row = -1;
        REQUIRE(QMetaObject::invokeMethod(tree_view, "rowAtIndex",
            Q_RETURN_ARG(int, row), Q_ARG(QModelIndex, index)));
        return row;
    };
    auto is_expanded = [&](int row) {
        bool expanded = false;
        REQUIRE(QMetaObject::invokeMethod(tree_view, "isExpanded",
            Q_RETURN_ARG(bool, expanded), Q_ARG(int, row)));
        return expanded;
    };

    int resets = 0;
    QObject::connect(tree_model, &QAbstractItemModel::modelReset, [&] { ++resets; });

    const int initial_row = row_of(tree_model->findIndexByNodeId(target_component, 1));
    REQUIRE(initial_row >= 0);
    REQUIRE(is_expanded(initial_row));

    const Index third_model = model.addModel("third", make_components());
    QCoreApplication::processEvents();
    CHECK(resets == 0);
    int row = row_of(tree_model->findIndexByNodeId(target_component, 1));
    REQUIRE(row >= 0);
    CHECK(is_expanded(row));

    model.removeModel(first_model);
    QCoreApplication::processEvents();
    CHECK(resets == 0);
    row = row_of(tree_model->findIndexByNodeId(target_component, 1));
    REQUIRE(row >= 0);
    CHECK(is_expanded(row));

    model.removeModel(third_model);
    QCoreApplication::processEvents();
    CHECK(resets == 0);
    row = row_of(tree_model->findIndexByNodeId(target_component, 1));
    REQUIRE(row >= 0);
    CHECK(is_expanded(row));
}

TEST_CASE("Actual QML expands and collapses object tree subtrees", "[QML][tree]")
{
    application();
    QmlFixture f;
    auto& model = *f.manager->getModelManager();
    model.setUndoRecorder(nullptr);
    auto* tree_model = f.tree->findChild<TreeModel*>();
    auto* tree_view = f.tree->findChild<QObject*>("objectTreeView");
    REQUIRE(tree_model);
    REQUIRE(tree_view);

    auto make_component = [](bool with_solid, bool with_geometry) {
        auto component = std::make_unique<ComponentData>();
        component->mesh = std::make_unique<MeshData>();
        component->mesh->vertex_positions_ = { { 0.0, 0.0, 0.0 }, { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 } };
        component->mesh->vertex_count_ = 3;
        component->mesh->face_vertices_offset_.assign(2, 0);
        if (with_solid)
            component->mesh->solid_types_.assign(1, 0);
        if (with_geometry)
            component->geometry = std::make_unique<GeometryData>();
        return component;
    };

    ComponentDatas components_a;
    components_a.push_back(make_component(true, true));
    const Index model_a = model.addModel("levels-a", std::move(components_a));
    ComponentDatas components_b;
    components_b.push_back(make_component(false, false));
    const Index model_b = model.addModel("levels-b", std::move(components_b));
    REQUIRE(tree_model->refresh());

    const Index component_a = model.modelById(model_a)->componentIds().front();
    const Index component_b = model.modelById(model_b)->componentIds().front();
    const QModelIndex model_a_index = tree_model->findIndexByNodeId(model_a, 0);
    const QModelIndex model_b_index = tree_model->findIndexByNodeId(model_b, 0);
    const QModelIndex component_a_index = tree_model->findIndexByNodeId(component_a, 1);
    const QModelIndex component_b_index = tree_model->findIndexByNodeId(component_b, 1);
    REQUIRE(model_a_index.isValid());
    REQUIRE(model_b_index.isValid());
    REQUIRE(component_a_index.isValid());
    REQUIRE(component_b_index.isValid());
    const QModelIndex mesh_a_index = tree_model->index(0, 0, component_a_index);
    const QModelIndex mesh_b_index = tree_model->index(0, 0, component_b_index);
    REQUIRE(mesh_a_index.isValid());
    REQUIRE(mesh_b_index.isValid());
    const QModelIndex mesh_child_index = tree_model->index(0, 0, mesh_a_index);
    REQUIRE(mesh_child_index.isValid());

    auto settle = [&] {
        REQUIRE(QMetaObject::invokeMethod(tree_view, "forceLayout"));
        QCoreApplication::processEvents();
    };
    auto row_of = [&](const QModelIndex& index) {
        int row = -1;
        REQUIRE(QMetaObject::invokeMethod(tree_view, "rowAtIndex",
            Q_RETURN_ARG(int, row), Q_ARG(QModelIndex, index)));
        return row;
    };
    auto is_expanded = [&](int row) {
        bool expanded = false;
        REQUIRE(QMetaObject::invokeMethod(tree_view, "isExpanded",
            Q_RETURN_ARG(bool, expanded), Q_ARG(int, row)));
        return expanded;
    };
    auto rows = [&] { return tree_view->property("rows").toInt(); };
    auto expand_subtree = [&](const QModelIndex& index) {
        const int row = row_of(index);
        REQUIRE(row >= 0);
        REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "expandSubtree", Q_ARG(QVariant, row)));
        settle();
    };
    auto collapse_subtree = [&](const QModelIndex& index) {
        const int row = row_of(index);
        REQUIRE(row >= 0);
        REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "collapseSubtree", Q_ARG(QVariant, row)));
        settle();
    };
    auto expand_all = [&] {
        REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "expandSubtree", Q_ARG(QVariant, -1)));
        settle();
    };
    auto collapse_all = [&] {
        REQUIRE(QMetaObject::invokeMethod(f.tree.get(), "collapseSubtree", Q_ARG(QVariant, -1)));
        settle();
    };

    REQUIRE(QMetaObject::invokeMethod(tree_view, "collapseRecursively", Q_ARG(int, -1)));
    settle();
    CHECK(rows() == 2);

    // 全部展开模型子树：递归到所有叶子，但不影响其他模型。
    expand_subtree(model_a_index);
    REQUIRE(row_of(model_a_index) >= 0);
    CHECK(is_expanded(row_of(model_a_index)));
    REQUIRE(row_of(component_a_index) >= 0);
    CHECK(is_expanded(row_of(component_a_index)));
    REQUIRE(row_of(mesh_a_index) >= 0);
    CHECK(is_expanded(row_of(mesh_a_index)));
    REQUIRE(row_of(mesh_child_index) >= 0);
    CHECK_FALSE(is_expanded(row_of(mesh_child_index)));
    REQUIRE(row_of(model_b_index) >= 0);
    CHECK_FALSE(is_expanded(row_of(model_b_index)));
    CHECK(rows() == 7);

    // 折叠组件子树只收起该组件及其后代。
    collapse_subtree(component_a_index);
    REQUIRE(row_of(component_a_index) >= 0);
    CHECK_FALSE(is_expanded(row_of(component_a_index)));
    CHECK(row_of(mesh_a_index) == -1);
    CHECK(rows() == 3);

    expand_subtree(component_a_index);
    REQUIRE(row_of(mesh_a_index) >= 0);
    CHECK(is_expanded(row_of(mesh_a_index)));
    CHECK(rows() == 7);

    collapse_subtree(mesh_a_index);
    REQUIRE(row_of(mesh_a_index) >= 0);
    CHECK_FALSE(is_expanded(row_of(mesh_a_index)));
    CHECK(row_of(mesh_child_index) == -1);
    CHECK(rows() == 5);

    expand_subtree(mesh_a_index);
    REQUIRE(row_of(mesh_child_index) >= 0);
    CHECK(rows() == 7);

    collapse_subtree(model_a_index);
    CHECK(row_of(component_a_index) == -1);
    CHECK(rows() == 2);

    expand_subtree(model_b_index);
    REQUIRE(row_of(model_b_index) >= 0);
    CHECK(is_expanded(row_of(model_b_index)));
    REQUIRE(row_of(component_b_index) >= 0);
    CHECK(is_expanded(row_of(component_b_index)));
    REQUIRE(row_of(mesh_b_index) >= 0);
    CHECK(is_expanded(row_of(mesh_b_index)));
    CHECK(rows() == 5);

    collapse_subtree(model_b_index);
    CHECK(rows() == 2);

    // 空白区全局展开/折叠作用于所有模型。
    expand_all();
    CHECK(rows() == 10);
    CHECK(is_expanded(row_of(model_a_index)));
    CHECK(is_expanded(row_of(model_b_index)));

    collapse_all();
    CHECK(rows() == 2);

    model.removeModel(model_a);
    model.removeModel(model_b);
    REQUIRE(waitForCondition([&] { return tree_model->rowCount() == 0; }));
    settle();
    CHECK(rows() == 0);
}

TEST_CASE("Actual undo clears offscreen selectors only after successful restoration", "[QML][undo]")
{
    application();
    QmlFixture f;
    auto* manager = f.manager;
    auto* app = f.app;
    auto* selection_state = f.selection_state;
    auto& model = *manager->getModelManager();
    auto& system = *manager->getFeatureSystemAdaptor()->featureSystem();
    systems::feature::HandlerMetaData meta;
    meta.name = "ManySelectors";
    REQUIRE(system.registerHandler(meta, systems::feature::FeatureSystem::SystemHandlerPtr { std::make_unique<ManySelectors>().release() }));
    auto* info = manager->getFeatureSystemAdaptor()->getFeaturesInfo().front();
    QJSValue active = f.engine.newObject();
    // 普通功能也使用统一入口身份；视口外参数仍由同一会话模型清理。
    active.setProperty("entryId", QStringLiteral("feature:ManySelectors:功能"));
    active.setProperty("isFeature", true);
    active.setProperty("info", f.engine.newQObject(info));
    app->setProperty("activeOperation", QVariant::fromValue(active));
    QSelection selection(std::make_unique<Selection>());
    selection.get()->ids = { 123 };
    selection.get()->type = ElementEnum::Face;
    for (int i = 0; i < 50; ++i)
        REQUIRE(manager->getFeatureSystemAdaptor()->setParameter("ManySelectors", i, QVariant::fromValue(&selection)));
    selection_state->setProperty("listeningSelectorIndex", 49);
    const auto revision = selection_state->property("selectionRevision").toInt();
    auto* undo = manager->getUndoStackAdaptor();
    int applied = 0;
    QObject::connect(undo, &QUndoStackAdaptor::applied, [&] { ++applied; });
    // 空历史和忙时拒绝都不得触发选择清理。
    undo->undo();
    CHECK(applied == 0);
    const Index mid = std::any_cast<Index>(system.invoke("ManySelectors"));
    REQUIRE(model.modelById(mid));
    REQUIRE(undo->canUndo());
    auto operation = model.beginWriteOperation(false);
    undo->undo();
    CHECK(applied == 0);
    CHECK(model.modelById(mid));
    CHECK(selection_state->property("listeningSelectorIndex").toInt() == 49);
    CHECK(selection_state->property("selectionRevision").toInt() == revision);
    for (int i = 0; i < 50; ++i)
        CHECK(*system.params("ManySelectors")->value(i).get<ArgTypeEnum::Selector>());
    operation.reset();
    undo->undo();
    CHECK(applied == 1);
    CHECK_FALSE(model.modelById(mid));
    CHECK(selection_state->property("listeningSelectorIndex").toInt() == -1);
    CHECK(selection_state->property("selectionRevision").toInt() > revision);
    for (int i = 0; i < 50; ++i)
        CHECK_FALSE(*system.params("ManySelectors")->value(i).get<ArgTypeEnum::Selector>());
    // redo 同样经真实恢复路径触发清理。
    REQUIRE(undo->canRedo());
    REQUIRE(manager->getFeatureSystemAdaptor()->setParameter("ManySelectors", 49, QVariant::fromValue(&selection)));
    selection_state->setProperty("listeningSelectorIndex", 49);
    undo->redo();
    CHECK(applied == 2);
    CHECK(model.modelById(mid));
    CHECK(selection_state->property("listeningSelectorIndex").toInt() == -1);
    CHECK_FALSE(*system.params("ManySelectors")->value(49).get<ArgTypeEnum::Selector>());
}
