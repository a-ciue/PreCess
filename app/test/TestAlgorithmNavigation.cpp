/** @file TestAlgorithmNavigation.cpp
 * @brief 算法导航的分类兼容、排序及实际工具栏加载测试
 */
#include "AlgorithmHandler.h"
#include "AlgorithmSystem.h"
#include "AlgorithmSystemRegister.h"
#include "ComponentData.h"
#include "FeatureContext.h"
#include "FeatureEvents.h"
#include "FeatureHandler.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "FeatureSystem.h"
#include "GeometryData.h"
#include "HandlerCreatorDestroyerFactory.h"
#include "ModelIOSystem.h"
#include "PluginBase.h"
#include "QFeatureInfo.h"
#include "QModelManager.h"
#include "QSelection.h"
#include "QTaskStatus.h"

#include <QEventLoop>
#include <QFile>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJSEngine>
#include <QJsonArray>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTimer>
#include <TopoDS_Shape.hxx>
#include <catch2/catch_test_macros.hpp>
#include <memory>

Q_IMPORT_QML_PLUGIN(app_corePlugin)
Q_IMPORT_QML_PLUGIN(app_modelPlugin)
Q_IMPORT_QML_PLUGIN(app_renderPlugin)
Q_IMPORT_QML_PLUGIN(app_model_systemsPlugin)
Q_IMPORT_QML_PLUGIN(app_model_systems_algoPlugin)
Q_IMPORT_QML_PLUGIN(app_model_systems_ioPlugin)
Q_IMPORT_QML_PLUGIN(app_model_systems_editPlugin)
Q_IMPORT_QML_PLUGIN(app_model_systems_featurePlugin)

namespace {
// 测试描述使用 Feature 包装，参数排列便于构造多分类样例。
class NavigationTestInfo : public QFeatureInfo {
public:
    NavigationTestInfo(QString name, QString display_name, QString description, QList<QArgType*> args,
        QObject* parent = nullptr, QStringList categories = {}, QString group = {}, QString icon = {},
        int order = 0, QString label = {}, QVariantMap defaults = {})
        : QFeatureInfo(std::move(name), std::move(display_name), std::move(description), "", std::move(icon),
              std::move(args), false, parent, std::move(categories), std::move(group), order, std::move(label), std::move(defaults))
    {
    }
};
QQuickItem* findItem(QQuickItem* root, const QString& name)
{
    if (root->objectName() == name)
        return root;
    for (auto* child : root->childItems()) {
        if (auto* found = findItem(child, name))
            return found;
    }
    return nullptr;
}
QQuickItem* findAction(QQuickItem* root, const QString& text)
{
    if (root->property("text").toString() == text)
        return root;
    for (auto* child : root->childItems()) {
        if (auto* found = findAction(child, text))
            return found;
    }
    return nullptr;
}
void clickItem(QQuickWindow& window, QQuickItem* item)
{
    const QPointF position = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
    const QPointF global = window.mapToGlobal(position.toPoint());
    QMouseEvent press(QEvent::MouseButtonPress, position, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, position, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &release);
}
void pressKey(QQuickWindow& window, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier, const QString& text = {})
{
    QKeyEvent press(QEvent::KeyPress, key, modifiers, text);
    QCoreApplication::sendEvent(&window, &press);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers, text);
    QCoreApplication::sendEvent(&window, &release);
}
std::unique_ptr<QObject> createSession(QQmlEngine& engine, const QJSValue& algorithm_system = {})
{
    QQmlComponent component(&engine, QUrl("qrc:/navigation-ui/OperationSession.qml"));
    INFO(component.errorString().toStdString());
    REQUIRE(component.isReady());
    std::unique_ptr<QObject> session(algorithm_system.isObject()
            ? component.createWithInitialProperties({ { "meshFeatureSystem", QVariant::fromValue(algorithm_system) } })
            : component.create());
    INFO(component.errorString().toStdString());
    REQUIRE(session);
    return session;
}
class ButtonProbe : public systems::feature::FeatureHandler {
public:
    explicit ButtonProbe(int& presses)
        : presses_(presses)
    {
    }
    void setup(systems::feature::FeatureRegistrar& reg, systems::feature::FeatureContext& ctx) override
    {
        reg.addParameter({ ArgTypeEnum::Button, "触发", "" });
        subscription_ = ctx.events.subscribe<systems::feature::ParameterChangedEvent>([this](const auto&) { ++presses_; });
    }

private:
    int& presses_;
    core::EventBus::Subscription subscription_;
};
struct ParameterProbeState {
    int notifications { 0 };
    int activations { 0 };
    double activated_value { 0 };
};
class ParameterProbe : public systems::feature::FeatureHandler {
public:
    ParameterProbe(std::vector<core::ArgType> types, ParameterProbeState& state)
        : types_(std::move(types))
        , state_(state)
    {
    }
    void setup(systems::feature::FeatureRegistrar& reg, systems::feature::FeatureContext& ctx) override
    {
        for (const auto& type : types_)
            reg.addParameter(type);
        subscription_ = ctx.events.subscribe<systems::feature::ParameterChangedEvent>([this](const auto&) { ++state_.notifications; });
    }
    void activate(systems::feature::FeatureContext& ctx) override
    {
        ++state_.activations;
        if (const auto* value = ctx.params.value(0).get<ArgTypeEnum::Float>())
            state_.activated_value = *value;
        if (ctx.params.count() > 4)
            ctx.params.setValue(4, core::ArgObject::create<ArgTypeEnum::Float>(7.0));
    }

private:
    std::vector<core::ArgType> types_;
    ParameterProbeState& state_;
    core::EventBus::Subscription subscription_;
};
void application()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    static int argc = 1;
    static char name[] = "TestAlgorithmNavigation";
    static char* argv[] = { name, nullptr };
    static QGuiApplication app(argc, argv);
    if (const auto font_path = qEnvironmentVariable("PRECESS_NAVIGATION_FONT"); !font_path.isEmpty()) {
        const auto families = QFontDatabase::applicationFontFamilies(QFontDatabase::addApplicationFont(font_path));
        if (!families.isEmpty())
            QGuiApplication::setFont(QFont(families.front(), 10));
    }
}
}

TEST_CASE("Algorithm navigation groups declared categories and preserves unclassified algorithms", "[navigation]")
{
    application();
    QJSEngine engine;
    QFile source(":/navigation-ui/AlgorithmNavigation.js");
    REQUIRE(source.open(QIODevice::ReadOnly));
    REQUIRE_FALSE(engine.evaluate(QString::fromUtf8(source.readAll()).remove(".pragma library")).isError());
    auto result = engine.evaluate(R"JS(
        const infos = [
            {name: "z", categories: ["triangle"], group: "Generate", order: 0},
            {name: "multi", categories: ["triangle", "tetrahedron"], group: "Generate", order: -1},
            {name: "a", categories: ["triangle", "triangle"], group: "Generate", order: 0},
            {name: "legacy"},
            {name: "future", categories: []},
            {name: "mixed", categories: ["hexahedron"], group: "Other group"}
        ];
        JSON.stringify({
            triangle: buildGroups(infos, "triangle").map(g => g.items.map(i => i.name)),
            tetrahedron: buildGroups(infos, "tetrahedron").map(g => g.items.map(i => i.name)),
            quadrilateral: buildGroups(infos, "quadrilateral"),
            other: buildGroups(infos, "other").map(g => g.items.map(i => i.name)),
            hexahedron: buildGroups(infos, "hexahedron").map(g => g.name)
        })
    )JS");
    INFO(result.toString().toStdString());
    REQUIRE_FALSE(result.isError());
    CHECK(result.toString() == R"({"triangle":[["multi","a","z"]],"tetrahedron":[["multi"]],"quadrilateral":[],"other":[["future","legacy"]],"hexahedron":["Other group"]})");
    const auto ordered = engine.evaluate(R"JS(
        buildAlgorithms([
            {name: "third", categories: ["triangle"], group: "A", order: 3},
            {name: "second", categories: ["triangle"], group: "B", order: 2},
            {name: "first", categories: ["triangle"], group: "A", order: 1}
        ], "triangle").map(info => info.name).join(",")
    )JS");
    CHECK(ordered.toString() == "first,second,third");
}

TEST_CASE("Qt feature information exposes navigation as JavaScript arrays", "[navigation][Qt]")
{
    application();
    QJSEngine engine;
    QFile source(":/navigation-ui/AlgorithmNavigation.js");
    REQUIRE(source.open(QIODevice::ReadOnly));
    REQUIRE_FALSE(engine.evaluate(QString::fromUtf8(source.readAll()).remove(".pragma library")).isError());
    NavigationTestInfo info("Mesher", "Mesh generator", "", {}, nullptr,
        { "triangle", "tetrahedron" }, "Generate", "qrc:/plugin/mesher.svg", -10);
    engine.globalObject().setProperty("info", engine.newQObject(&info));
    auto result = engine.evaluate(R"JS(
        JSON.stringify({
            names: buildGroups([info], "tetrahedron")[0].items.map(i => i.name),
            group: info.group, icon: info.icon, order: info.order,
            other: buildGroups([info], "other").length
        })
    )JS");
    INFO(result.toString().toStdString());
    REQUIRE_FALSE(result.isError());
    CHECK(info.label() == "Mesh generator");
    CHECK(result.toString() == R"({"names":["Mesher"],"group":"Generate","icon":"qrc:/plugin/mesher.svg","order":-10,"other":0})");
}

TEST_CASE("Actual toolbar renders supplied mesh categories and preserves active operation", "[navigation][QML]")
{
    application();
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto executable = (directory.path() + "/isolated/Test.exe").toStdString();
    QModelManager::argv0 = executable;
    QQmlEngine engine;
    static const int theme_type = qmlRegisterSingletonType(QUrl("qrc:/navigation-ui/Theme.qml"), "NavigationUi", 1, 0, "Theme");
    engine.rootContext()->setContextProperty("Theme", engine.singletonInstance<QObject*>(theme_type));
    QQmlComponent component(&engine, QUrl("qrc:/navigation-ui/AppToolbar.qml"));
    INFO(component.errorString().toStdString());
    REQUIRE(component.isReady());
    auto* manager = engine.singletonInstance<QModelManager*>("app.model", "QModelManager");
    auto* feature_adaptor = manager->getFeatureSystemAdaptor();
    auto& feature_system = *feature_adaptor->featureSystem();
    auto provider = engine.newQObject(feature_adaptor);
    std::unique_ptr<QObject> toolbar(component.createWithInitialProperties({ { "meshFeatureSystem", QVariant::fromValue(provider) } }));
    INFO(component.errorString().toStdString());
    REQUIRE(toolbar);
    auto* item = qobject_cast<QQuickItem*>(toolbar.get());
    REQUIRE(item);
    QQuickWindow window;
    window.resize(800, 180);
    item->setParentItem(window.contentItem());
    item->setWidth(800);
    item->setHeight(item->implicitHeight());
    QObject::connect(item, &QQuickItem::implicitHeightChanged, item, [item] { item->setHeight(item->implicitHeight()); });
    window.show();
    {
        QEventLoop loop;
        QTimer::singleShot(50, &loop, &QEventLoop::quit);
        loop.exec();
    }
    auto* app = engine.singletonInstance<QObject*>("app.core", "App");
    REQUIRE(app);
    auto operation = engine.newObject();
    operation.setProperty("marker", 42);
    REQUIRE(app->setProperty("activeOperation", QVariant::fromValue(operation)));
    auto* tab = findItem(item, "meshGenerationTab");
    REQUIRE(tab);
    REQUIRE(tab->setProperty("checked", true));
    REQUIRE(QMetaObject::invokeMethod(tab, "clicked"));
    CHECK(toolbar->property("activeCategory").toInt() == 2);
    CHECK(app->property("activeOperation").value<QJSValue>().strictlyEquals(operation));

    // 窄窗口保留四类入口，通过横向滚动访问完整文字。
    item->setWidth(300);
    QCoreApplication::processEvents();
    auto* categories = findItem(item, "meshCategoryPage");
    REQUIRE(categories);
    CHECK(categories->property("contentWidth").toReal() > categories->width());
    item->setWidth(800);

    auto session = createSession(engine, provider);
    QQmlComponent sidebar_component(&engine, QUrl("qrc:/navigation-ui/SideBar.qml"));
    INFO(sidebar_component.errorString().toStdString());
    REQUIRE(sidebar_component.isReady());
    std::unique_ptr<QObject> sidebar(sidebar_component.createWithInitialProperties({ { "session", QVariant::fromValue(session.get()) } }));
    REQUIRE(sidebar);
    auto* sidebar_item = qobject_cast<QQuickItem*>(sidebar.get());
    REQUIRE(sidebar_item);
    window.resize(1000, 700);
    sidebar_item->setParentItem(window.contentItem());
    sidebar_item->setY(180);
    sidebar_item->setWidth(360);
    sidebar_item->setHeight(500);

    const core::ArgType size_type { ArgTypeEnum::Float, "目标尺寸", "1", "" };
    const core::ArgType quality_type { ArgTypeEnum::Bool, "质量优化", "true", "" };
    const core::ArgType iterations_type { ArgTypeEnum::Int, "迭代次数", "3", "" };
    QArgType size(size_type);
    QArgType quality(quality_type);
    QArgType iterations(iterations_type);
    ParameterProbeState first_state, second_state;
    REQUIRE(feature_system.registerHandler({ .name = "first" },
        systems::feature::FeatureSystem::SystemHandlerPtr { std::make_unique<ParameterProbe>(std::vector<core::ArgType> { size_type, quality_type }, first_state).release() }));
    REQUIRE(feature_system.registerHandler({ .name = "second" },
        systems::feature::FeatureSystem::SystemHandlerPtr { std::make_unique<ParameterProbe>(std::vector<core::ArgType> { iterations_type }, second_state).release() }));
    NavigationTestInfo first("first", "Legacy library name", "", { &size, &quality }, nullptr,
        { "triangle", "tetrahedron" }, "", "", 0, "网格生成（德劳内方法）");
    NavigationTestInfo second("second", "Second library", "", { &iterations }, nullptr,
        { "triangle" }, "", "", 1, "三角形网格生成（波前推进法）");
    auto infos = engine.newArray(2);
    infos.setProperty(0, engine.newQObject(&first));
    infos.setProperty(1, engine.newQObject(&second));
    REQUIRE(session->setProperty("meshFeatureInfos", QVariant::fromValue(infos)));
    auto* triangle = findItem(item, "algorithmCategory_triangle");
    REQUIRE(triangle);
    REQUIRE(QMetaObject::invokeMethod(triangle, "clicked"));
    {
        QEventLoop loop;
        QTimer::singleShot(50, &loop, &QEventLoop::quit);
        loop.exec();
    }
    CHECK(app->property("activeOperation").value<QJSValue>().property("info").property("name").toString() == "first");
    auto* selector = findItem(sidebar_item, "meshAlgorithmSelector");
    REQUIRE(selector);
    CHECK(sidebar->property("panelTitle").toString() == "操作面板-三角形网格生成");
    CHECK(selector->property("count").toInt() == 2);
    CHECK(selector->property("visible").toBool());
    CHECK(selector->property("currentText").toString() == first.label());
    REQUIRE(QMetaObject::invokeMethod(sidebar.get(), "setParam", Q_ARG(QVariant, 0), Q_ARG(QVariant, 9.0)));
    REQUIRE(QMetaObject::invokeMethod(triangle, "clicked"));
    CHECK(sidebar->property("parameters").value<QJSValue>().property(0).toNumber() == 9.0);
    auto* selection = app->property("selection").value<QObject*>();
    REQUIRE(selection);
    selection->setProperty("listeningSelectorIndex", 0);
    // 无关插件增删和描述包装刷新不能重新开始当前操作。
    NavigationTestInfo unrelated("unrelated", "其他算法", "", {});
    NavigationTestInfo refreshed_first("first", "Legacy library name", "", { &size, &quality }, nullptr,
        { "triangle", "tetrahedron" }, "", "", 0, "更新后的算法名称");
    auto refreshed = engine.newArray(3);
    refreshed.setProperty(0, engine.newQObject(&refreshed_first));
    refreshed.setProperty(1, engine.newQObject(&second));
    refreshed.setProperty(2, engine.newQObject(&unrelated));
    REQUIRE(session->setProperty("meshFeatureInfos", QVariant::fromValue(refreshed)));
    {
        QEventLoop loop;
        QTimer::singleShot(50, &loop, &QEventLoop::quit);
        loop.exec();
    }
    CHECK(sidebar->property("parameters").value<QJSValue>().property(0).toNumber() == 9.0);
    CHECK(selection->property("listeningSelectorIndex").toInt() == 0);
    CHECK(selector->property("currentText").toString() == refreshed_first.label());
    REQUIRE(session->setProperty("meshFeatureInfos", QVariant::fromValue(infos)));
    {
        QEventLoop loop;
        QTimer::singleShot(50, &loop, &QEventLoop::quit);
        loop.exec();
    }
    CHECK(sidebar->property("parameters").value<QJSValue>().property(0).toNumber() == 9.0);
    CHECK(selection->property("listeningSelectorIndex").toInt() == 0);
    // 会话由宿主持有，销毁并重建整个面板也不能丢失参数或选择监听。
    sidebar.reset();
    QCoreApplication::processEvents();
    auto* parameter_model = session->property("parameterModel").value<QObject*>();
    REQUIRE(parameter_model);
    CHECK(parameter_model->property("values").value<QJSValue>().property(0).toNumber() == 9.0);
    CHECK(selection->property("listeningSelectorIndex").toInt() == 0);
    sidebar.reset(sidebar_component.createWithInitialProperties({ { "session", QVariant::fromValue(session.get()) } }));
    sidebar_item = qobject_cast<QQuickItem*>(sidebar.get());
    REQUIRE(sidebar_item);
    sidebar_item->setParentItem(window.contentItem());
    sidebar_item->setY(180);
    sidebar_item->setSize(QSizeF(360, 500));
    {
        QEventLoop loop;
        QTimer::singleShot(50, &loop, &QEventLoop::quit);
        loop.exec();
    }
    selector = findItem(sidebar_item, "meshAlgorithmSelector");
    REQUIRE(selector);
    CHECK(sidebar->property("parameters").value<QJSValue>().property(0).toNumber() == 9.0);
    CHECK(selection->property("listeningSelectorIndex").toInt() == 0);
    // 同名算法的参数声明发生变化时，则必须重新初始化。
    const core::ArgType changed_size_type { ArgTypeEnum::Float, "目标尺寸", "2", "" };
    QArgType changed_size(changed_size_type);
    REQUIRE(feature_system.registerHandler({ .name = "first" },
        systems::feature::FeatureSystem::SystemHandlerPtr { std::make_unique<ParameterProbe>(std::vector<core::ArgType> { changed_size_type, quality_type }, first_state).release() }));

    NavigationTestInfo changed_first("first", "Changed", "", { &changed_size, &quality }, nullptr, { "triangle", "tetrahedron" });
    auto changed_infos = engine.newArray(2);
    changed_infos.setProperty(0, engine.newQObject(&changed_first));
    changed_infos.setProperty(1, engine.newQObject(&second));
    REQUIRE(session->setProperty("meshFeatureInfos", QVariant::fromValue(changed_infos)));
    {
        QEventLoop loop;
        QTimer::singleShot(50, &loop, &QEventLoop::quit);
        loop.exec();
    }
    CHECK(sidebar->property("parameters").value<QJSValue>().property(0).toNumber() == 2.0);
    CHECK(selection->property("listeningSelectorIndex").toInt() == -1);
    REQUIRE(QMetaObject::invokeMethod(selector, "activated", Q_ARG(int, 1)));
    {
        QEventLoop loop;
        QTimer::singleShot(50, &loop, &QEventLoop::quit);
        loop.exec();
    }
    CHECK(selection->property("listeningSelectorIndex").toInt() == -1);
    CHECK(app->property("activeOperation").value<QJSValue>().property("info").property("name").toString() == "second");
    CHECK(app->property("activeOperation").value<QJSValue>().property("execute").isCallable());
    CHECK(sidebar->property("parameters").value<QJSValue>().property("length").toInt() == 1);
    CHECK(sidebar->property("parameters").value<QJSValue>().property(0).toInt() == 3);

    // 顶层页签只改变展示，不清空当前参数；分类入口恢复上次算法。
    auto* edit = findItem(item, "editCategory");
    REQUIRE(edit);
    REQUIRE(QMetaObject::invokeMethod(edit, "clicked"));
    CHECK(app->property("activeOperation").value<QJSValue>().property("info").property("name").toString() == "second");
    REQUIRE(toolbar->setProperty("activeCategory", 2));
    for (const auto& category : { "quadrilateral", "tetrahedron", "hexahedron" }) {
        auto* button = findItem(item, QString("algorithmCategory_") + category);
        REQUIRE(button);
        CHECK(button->property("enabled").toBool());
        REQUIRE(QMetaObject::invokeMethod(button, "clicked"));
        {
            QEventLoop loop;
            QTimer::singleShot(50, &loop, &QEventLoop::quit);
            loop.exec();
        }
        CHECK(selector->property("count").toInt() == (QString(category) == "tetrahedron" ? 1 : 0));
        CHECK(selector->property("visible").toBool() == (QString(category) == "tetrahedron"));
    }
    REQUIRE(QMetaObject::invokeMethod(triangle, "clicked"));
    {
        QEventLoop loop;
        QTimer::singleShot(50, &loop, &QEventLoop::quit);
        loop.exec();
    }
    CHECK(app->property("activeOperation").value<QJSValue>().property("info").property("name").toString() == "second");
    // 动态卸载选择项后切到可用项，全部移除时不再持有旧参数和执行闭包。
    auto remaining = engine.newArray(1);
    remaining.setProperty(0, engine.newQObject(&first));
    REQUIRE(session->setProperty("meshFeatureInfos", QVariant::fromValue(remaining)));
    {
        QEventLoop loop;
        QTimer::singleShot(50, &loop, &QEventLoop::quit);
        loop.exec();
    }
    CHECK(app->property("activeOperation").value<QJSValue>().property("info").property("name").toString() == "first");
    CHECK(selector->property("visible").toBool());
    if (const auto capture = qEnvironmentVariable("PRECESS_NAVIGATION_CAPTURE"); !capture.isEmpty()) {
        QEventLoop loop;
        QTimer::singleShot(150, &loop, &QEventLoop::quit);
        loop.exec();
        REQUIRE(window.grabWindow().save(capture + ".png"));
    }
    REQUIRE(session->setProperty("meshFeatureInfos", QVariant::fromValue(engine.newArray())));
    {
        QEventLoop loop;
        QTimer::singleShot(50, &loop, &QEventLoop::quit);
        loop.exec();
    }
    CHECK(app->property("activeOperation").value<QJSValue>().property("info").isNull());
    CHECK_FALSE(selector->property("enabled").toBool());
    // 网格生成与普通操作反复切换时，执行按钮不能同时保留上下锚点。
    auto* buttons = findItem(sidebar_item, "operationButtons");
    auto* parameters = findItem(sidebar_item, "operationParameters");
    REQUIRE(buttons);
    REQUIRE(parameters);
    for (int cycle = 0; cycle < 3; ++cycle) {
        auto normal_operation = engine.newObject();
        normal_operation.setProperty("info", engine.newQObject(&first));
        REQUIRE(app->setProperty("activeOperation", QVariant::fromValue(normal_operation)));
        {
            QEventLoop loop;
            QTimer::singleShot(50, &loop, &QEventLoop::quit);
            loop.exec();
        }
        CHECK(buttons->height() <= 40);
        CHECK(buttons->y() < 20);
        CHECK(sidebar->property("panelTitle").toString() == "操作面板");
        CHECK(parameters->height() > 300);
        REQUIRE(QMetaObject::invokeMethod(triangle, "clicked"));
        {
            QEventLoop loop;
            QTimer::singleShot(50, &loop, &QEventLoop::quit);
            loop.exec();
        }
        CHECK(buttons->height() <= 40);
        CHECK(buttons->y() > 400);
    }
    app->setProperty("activeOperation", QVariant::fromValue(QJSValue(QJSValue::NullValue)));
    QModelManager::argv0 = {};
}

TEST_CASE("Installed external algorithms appear in the actual toolbar and open their parameters", "[navigation][external]")
{
    application();
    const auto host_override = qEnvironmentVariable("PRECESS_EXTERNAL_PLUGIN_HOST");
    // 测试程序与宿主共用运行目录；普通 ctest 也应验证已部署的外部插件。
    const auto host = (host_override.isEmpty() ? QCoreApplication::applicationFilePath() : host_override).toStdString();
    QModelManager::argv0 = host;
    QQmlEngine engine;
    static const int theme_type = qmlRegisterSingletonType(QUrl("qrc:/navigation-ui/Theme.qml"), "ExternalNavigationUi", 1, 0, "Theme");
    engine.rootContext()->setContextProperty("Theme", engine.singletonInstance<QObject*>(theme_type));
    QQmlComponent component(&engine, QUrl("qrc:/navigation-ui/AppToolbar.qml"));
    INFO(component.errorString().toStdString());
    REQUIRE(component.isReady());
    std::unique_ptr<QObject> toolbar(component.create());
    INFO(component.errorString().toStdString());
    REQUIRE(toolbar);
    const auto infos = engine.newQObject(engine.singletonInstance<QModelManager*>("app.model", "QModelManager")->getFeatureSystemAdaptor()).property("featuresInfo");
    const auto has_algorithm = [&](const QString& name) {
        for (int i = 0; i < infos.property("length").toInt(); ++i) {
            if (infos.property(i).property("name").toString() == name)
                return true;
        }
        return false;
    };
    const bool has_addons = has_algorithm("TetGenLibPlugin") && has_algorithm("GmshPlugin");
    if (!has_addons && host_override.isEmpty()) {
        QModelManager::argv0 = {};
        SKIP("Optional TetGenLib and Gmsh addon binaries are not installed beside the test executable");
    }
    REQUIRE(has_addons);
    auto* item = qobject_cast<QQuickItem*>(toolbar.get());
    REQUIRE(item);
    auto session = createSession(engine);
    QQmlComponent sidebar_component(&engine, QUrl("qrc:/navigation-ui/SideBar.qml"));
    INFO(sidebar_component.errorString().toStdString());
    REQUIRE(sidebar_component.isReady());
    std::unique_ptr<QObject> sidebar(sidebar_component.createWithInitialProperties({ { "session", QVariant::fromValue(session.get()) } }));
    REQUIRE(sidebar);
    auto* sidebar_item = qobject_cast<QQuickItem*>(sidebar.get());
    REQUIRE(sidebar_item);
    QQuickWindow window;
    window.resize(1000, 700);
    item->setParentItem(window.contentItem());
    item->setWidth(1000);
    item->setHeight(item->implicitHeight());
    sidebar_item->setParentItem(window.contentItem());
    sidebar_item->setY(180);
    sidebar_item->setWidth(360);
    sidebar_item->setHeight(500);
    window.show();
    auto* tetrahedron = findItem(item, "algorithmCategory_tetrahedron");
    REQUIRE(tetrahedron);
    REQUIRE(QMetaObject::invokeMethod(tetrahedron, "clicked"));
    const auto algorithms = sidebar->property("meshAlgorithms").value<QJSValue>();
    int index = -1;
    for (int i = 0; i < algorithms.property("length").toInt(); ++i) {
        if (algorithms.property(i).property("name").toString() == "TetGenLibPlugin")
            index = i;
    }
    REQUIRE(index >= 0);
    REQUIRE(QMetaObject::invokeMethod(session.get(), "selectMeshAlgorithm", Q_ARG(QVariant, index), Q_ARG(QVariant, false)));
    auto* app = engine.singletonInstance<QObject*>("app.core", "App");
    REQUIRE(app);
    const auto operation = app->property("activeOperation").value<QJSValue>();
    CHECK(operation.property("info").property("name").toString() == "TetGenLibPlugin");
    CHECK(operation.property("info").property("arg_types").property("length").toInt() > 0);
    CHECK(operation.property("execute").isCallable());
    auto* box = findAction(item, "创建长方体");
    REQUIRE(box);
    auto* buttons = findItem(sidebar_item, "operationButtons");
    auto* parameters = findItem(sidebar_item, "operationParameters");
    auto* selector = findItem(sidebar_item, "meshAlgorithmSelector");
    REQUIRE(buttons);
    REQUIRE(parameters);
    REQUIRE(selector);
    const auto settle = [] {
        QEventLoop loop;
        QTimer::singleShot(80, &loop, &QEventLoop::quit);
        loop.exec();
    };
    for (int cycle = 0; cycle < 3; ++cycle) {
        INFO(cycle);
        REQUIRE(QMetaObject::invokeMethod(box, "clicked"));
        settle();
        CHECK(buttons->height() <= 40);
        CHECK(buttons->y() < 20);
        CHECK(sidebar->property("panelTitle").toString() == "操作面板");
        CHECK(parameters->height() > 300);
        CHECK(parameters->property("count").toInt() == 7);
        CHECK_FALSE(selector->property("visible").toBool());
        if (const auto capture = qEnvironmentVariable("PRECESS_NAVIGATION_CAPTURE"); !capture.isEmpty())
            REQUIRE(window.grabWindow().save(capture + "-box.png"));
        if (cycle == 0) {
            auto* write_target = findItem(sidebar_item, "parameterControl_6");
            REQUIRE(write_target);
            clickItem(window, write_target);
            settle();
            auto target_popup = engine.newQObject(write_target).property("popup");
            REQUIRE(target_popup.property("visible").toBool());
            const auto target_list = target_popup.property("contentItem");
            auto* new_model = qobject_cast<QQuickItem*>(target_list.property("itemAtIndex")
                    .callWithInstance(target_list, { QJSValue(2) })
                    .toQObject());
            REQUIRE(new_model);
            clickItem(window, new_model);
            settle();
            CHECK(write_target->property("currentIndex").toInt() == 2);
            CHECK(sidebar->property("parameters").value<QJSValue>().property(6).toInt() == 2);
            CHECK(sidebar->property("parameters").value<QJSValue>().property(2).toNumber() == 0);
            auto* manager = engine.singletonInstance<QModelManager*>("app.model", "QModelManager");
            const auto* params = manager->getFeatureSystemAdaptor()->featureSystem()->params("CreateBox");
            REQUIRE(params);
            REQUIRE(params->value(6).get<ArgTypeEnum::Combo>());
            REQUIRE(*params->value(6).get<ArgTypeEnum::Combo>() == 2);
            // 没有对象树选中态时，经生产执行闭包真正创建几何，不能只断言控件文案。
            const auto result = app->property("activeOperation").value<QJSValue>().property("execute").call();
            INFO(result.toString().toStdString());
            REQUIRE(result.isNumber());
            auto* component = manager->getModelManager()->findComponent(result.toInt());
            REQUIRE(component);
            REQUIRE(component->geometry);
            REQUIRE(component->geometry->rootShape);
            CHECK_FALSE(component->geometry->rootShape->IsNull());
        }
        REQUIRE(QMetaObject::invokeMethod(tetrahedron, "clicked"));
        settle();
        CHECK(buttons->height() <= 40);
        CHECK(buttons->y() > 400);
    }
    auto* triangle = findItem(item, "algorithmCategory_triangle");
    REQUIRE(triangle);
    REQUIRE(QMetaObject::invokeMethod(triangle, "clicked"));
    settle();
    CHECK(sidebar->property("meshAlgorithms").value<QJSValue>().property("length").toInt() == 1);
    CHECK(selector->property("visible").toBool());
    CHECK(parameters->property("count").toInt() > 0);
    REQUIRE(toolbar->setProperty("activeCategory", 2));
    settle();
    if (const auto capture = qEnvironmentVariable("PRECESS_NAVIGATION_CAPTURE"); !capture.isEmpty())
        REQUIRE(window.grabWindow().save(capture + "-single.png"));
    auto* quadrilateral = findItem(item, "algorithmCategory_quadrilateral");
    REQUIRE(quadrilateral);
    REQUIRE(QMetaObject::invokeMethod(quadrilateral, "clicked"));
    settle();
    CHECK(selector->property("count").toInt() == 1);
    CHECK(selector->property("visible").toBool());
    CHECK(selector->property("currentText").toString() == "曲面网格生成");
    CHECK(sidebar->property("parameters").value<QJSValue>().property(6).toInt() == 1);
    if (const auto capture = qEnvironmentVariable("PRECESS_NAVIGATION_CAPTURE"); !capture.isEmpty())
        REQUIRE(window.grabWindow().save(capture + "-quad.png"));
    // 使用窗口鼠标/键盘事件验证实际参数交互，不能只调用 activated 信号。
    auto* mode = findItem(sidebar_item, "parameterControl_1");
    auto* size = findItem(sidebar_item, "parameterControl_2");
    auto* geometry = findItem(sidebar_item, "parameterControl_0");
    REQUIRE(mode);
    REQUIRE(size);
    REQUIRE(geometry);

    QPointer<QQuickItem> watched_mode(mode);
    const auto before = app->property("activeOperation").value<QJSValue>();
    clickItem(window, mode);
    settle();

    CHECK(app->property("activeOperation").value<QJSValue>().strictlyEquals(before));
    REQUIRE(watched_mode);
    auto* popup = engine.newQObject(mode).property("popup").toQObject();
    REQUIRE(popup);
    CHECK(popup->property("visible").toBool());
    const auto popup_list = engine.newQObject(popup).property("contentItem");
    auto* option = qobject_cast<QQuickItem*>(popup_list.property("itemAtIndex").callWithInstance(popup_list, { QJSValue(1) }).toQObject());
    REQUIRE(option);
    clickItem(window, option);
    settle();
    CHECK(mode->property("currentIndex").toInt() == 1);
    CHECK(sidebar->property("parameters").value<QJSValue>().property(1).toInt() == 1);
    clickItem(window, size);
    pressKey(window, Qt::Key_A, Qt::ControlModifier);
    pressKey(window, Qt::Key_2, Qt::NoModifier, "2");
    pressKey(window, Qt::Key_5, Qt::NoModifier, "5");
    pressKey(window, Qt::Key_Tab);
    settle();
    CHECK(size->property("text").toString() == "25");
    CHECK(sidebar->property("parameters").value<QJSValue>().property(2).toString() == "25");
    clickItem(window, geometry);
    settle();
    auto* selection = app->property("selection").value<QObject*>();
    REQUIRE(selection);
    CHECK(selection->property("listeningSelectorIndex").toInt() == 0);
    clickItem(window, geometry);
    settle();
    CHECK(selection->property("listeningSelectorIndex").toInt() == -1);
    app->setProperty("activeOperation", QVariant::fromValue(QJSValue(QJSValue::NullValue)));
    QModelManager::argv0 = {};
}

TEST_CASE("Actual parameter controls preserve edits through scrolling and accept external updates", "[navigation][parameters]")
{
    application();
    QTemporaryDir directory;
    REQUIRE(directory.isValid());
    const auto executable = (directory.path() + "/isolated/Test.exe").toStdString();
    QModelManager::argv0 = executable;
    QQmlEngine engine;
    static const int theme_type = qmlRegisterSingletonType(QUrl("qrc:/navigation-ui/Theme.qml"), "ParameterUi", 1, 0, "Theme");
    engine.rootContext()->setContextProperty("Theme", engine.singletonInstance<QObject*>(theme_type));
    auto session = createSession(engine);
    QQmlComponent component(&engine, QUrl("qrc:/navigation-ui/SideBar.qml"));
    INFO(component.errorString().toStdString());
    REQUIRE(component.isReady());
    QQuickWindow window;
    window.resize(500, 320);
    std::unique_ptr<QObject> sidebar(component.createWithInitialProperties({ { "session", QVariant::fromValue(session.get()) } }));
    auto* item = qobject_cast<QQuickItem*>(sidebar.get());
    REQUIRE(item);
    item->setParentItem(window.contentItem());
    item->setSize(QSizeF(500, 320));
    window.show();
    const auto settle = [] {
        QEventLoop loop;
        QTimer::singleShot(80, &loop, &QEventLoop::quit);
        loop.exec();
    };
    std::vector<core::ArgType> types {
        { ArgTypeEnum::Float, "数值", "1", "" },
        { ArgTypeEnum::Path, "路径", "default", "" },
        { ArgTypeEnum::Bool, "开关", "false", "" },
        { ArgTypeEnum::Selector, "目标", "Face", "" }
    };
    for (int i = 0; i < 40; ++i)
        types.push_back({ ArgTypeEnum::Float, "其他数值", "0", "" });
    std::vector<std::unique_ptr<QArgType>> wrappers;
    QList<QArgType*> args;
    for (const auto& type : types) {
        wrappers.push_back(std::make_unique<QArgType>(type));
        args.append(wrappers.back().get());
    }
    auto* manager = engine.singletonInstance<QModelManager*>("app.model", "QModelManager");
    auto& system = *manager->getFeatureSystemAdaptor()->featureSystem();
    ParameterProbeState probe_state;
    systems::feature::HandlerMetaData fixture_meta;
    fixture_meta.name = "fixture";
    REQUIRE(system.registerHandler(fixture_meta,
        systems::feature::FeatureSystem::SystemHandlerPtr { std::make_unique<ParameterProbe>(types, probe_state).release() }));
    QFeatureInfo info("fixture", "Fixture", "", "", "", args);
    auto* app = engine.singletonInstance<QObject*>("app.core", "App");
    auto operation = engine.newObject();
    operation.setProperty("info", engine.newQObject(&info));
    operation.setProperty("isFeature", true);
    const auto fixture_operation = operation;
    REQUIRE(app->setProperty("activeOperation", QVariant::fromValue(operation)));
    settle();
    // 完整默认值与可视行数量无关，尚未滚到的参数也已初始化。
    CHECK(sidebar->property("parameters").value<QJSValue>().property("length").toInt() == 44);
    CHECK(sidebar->property("parameters").value<QJSValue>().property(4).toNumber() == 7.0);
    CHECK(probe_state.activations == 1);
    CHECK(probe_state.notifications == 0);
    auto* numeric = findItem(item, "parameterControl_0");
    auto* path = findItem(item, "parameterControl_1");
    auto* toggle = findItem(item, "parameterControl_2");
    REQUIRE(numeric);
    REQUIRE(path);
    REQUIRE(toggle);
    clickItem(window, numeric);
    pressKey(window, Qt::Key_A, Qt::ControlModifier);
    pressKey(window, Qt::Key_Minus, Qt::NoModifier, "-");
    CHECK(numeric->property("text").toString() == "-");
    pressKey(window, Qt::Key_1, Qt::NoModifier, "1");
    pressKey(window, Qt::Key_Period, Qt::NoModifier, ".");
    CHECK(numeric->property("text").toString() == "-1.");
    pressKey(window, Qt::Key_5, Qt::NoModifier, "5");
    pressKey(window, Qt::Key_Tab);
    clickItem(window, path);
    pressKey(window, Qt::Key_A, Qt::ControlModifier);
    pressKey(window, Qt::Key_P, Qt::NoModifier, "picked");
    pressKey(window, Qt::Key_Tab);
    clickItem(window, toggle);
    settle();
    CHECK(sidebar->property("parameters").value<QJSValue>().property(0).toNumber() == -1.5);
    CHECK(sidebar->property("parameters").value<QJSValue>().property(1).toString() == "picked");
    CHECK(sidebar->property("parameters").value<QJSValue>().property(2).toBool());
    QSelection picked(std::make_unique<Selection>());
    picked.get()->type = ElementEnum::Face;
    picked.get()->ids = { 7 };
    REQUIRE(QMetaObject::invokeMethod(sidebar.get(), "setParam", Q_ARG(QVariant, 3), Q_ARG(QVariant, QVariant::fromValue(&picked))));
    QPointer<QQuickItem> old_numeric(numeric);
    auto* parameters = findItem(item, "operationParameters");
    REQUIRE(parameters);
    parameters->setProperty("currentIndex", -1);
    parameters->setProperty("cacheBuffer", 0);
    parameters->forceActiveFocus();
    REQUIRE(QMetaObject::invokeMethod(parameters, "positionViewAtEnd"));
    settle();
    CHECK_FALSE(old_numeric);
    REQUIRE(QMetaObject::invokeMethod(parameters, "positionViewAtBeginning"));
    settle();
    numeric = findItem(item, "parameterControl_0");
    path = findItem(item, "parameterControl_1");
    toggle = findItem(item, "parameterControl_2");
    auto* target = findItem(item, "parameterControl_3");
    REQUIRE(numeric);
    REQUIRE(path);
    REQUIRE(toggle);
    REQUIRE(target);
    CHECK(numeric->property("text").toString() == "-1.5");
    CHECK(path->property("text").toString() == "picked");
    CHECK(toggle->property("checked").toBool());
    auto* displayed_selection = qobject_cast<QSelection*>(target->parentItem()->property("value").value<QObject*>());
    REQUIRE(displayed_selection);
    CHECK(displayed_selection->get() == picked.get());
    const int before_update = probe_state.notifications;
    REQUIRE(manager->getFeatureSystemAdaptor()->setParameter("fixture", 0, 12.0));
    settle();
    CHECK(numeric->property("text").toString() == "12");
    CHECK(probe_state.notifications == before_update + 1);
    int presses = 0;
    systems::feature::HandlerMetaData meta;
    meta.name = "ButtonProbe";
    systems::feature::FeatureSystem::SystemHandlerPtr handler { std::make_unique<ButtonProbe>(presses).release() };
    REQUIRE(system.registerHandler(meta, std::move(handler)));
    const core::ArgType button_type { ArgTypeEnum::Button, "触发", "" };
    QArgType button_arg(button_type);
    QFeatureInfo button_info("ButtonProbe", "ButtonProbe", "", "", "", { &button_arg });
    operation = engine.newObject();
    operation.setProperty("info", engine.newQObject(&button_info));
    operation.setProperty("isFeature", true);
    REQUIRE(app->setProperty("activeOperation", QVariant::fromValue(operation)));
    settle();
    CHECK(presses == 0);
    auto* button = findItem(item, "parameterControl_0");
    REQUIRE(button);
    clickItem(window, button);
    settle();
    CHECK(presses == 1);
    // 重新进入功能读取当前参数，activate 也应看到这些值；进入不产生参数事件。
    const int before_reentry = probe_state.notifications;
    REQUIRE(app->setProperty("activeOperation", QVariant::fromValue(fixture_operation)));
    settle();
    CHECK(probe_state.activations == 2);
    CHECK(probe_state.activated_value == 12.0);
    CHECK(probe_state.notifications == before_reentry);
    CHECK(sidebar->property("parameters").value<QJSValue>().property(0).toNumber() == 12.0);
    CHECK(sidebar->property("parameters").value<QJSValue>().property(1).toString() == "picked");
    CHECK(sidebar->property("parameters").value<QJSValue>().property(2).toBool());
    app->setProperty("activeOperation", QVariant::fromValue(QJSValue(QJSValue::NullValue)));
    system.unregisterHandler(meta);
    system.unregisterHandler(fixture_meta);
    QModelManager::argv0 = {};
}

TEST_CASE("Registered custom categories create shared toolbar pages and disappear after the last provider", "[navigation][QML][registry]")
{
    application();
    QTemporaryDir directory;
    const auto executable = (directory.path() + "/isolated/Test.exe").toStdString();
    QModelManager::argv0 = executable;
    class CategoryHandler : public systems::feature::FeatureHandler {
    public:
        CategoryHandler(systems::feature::FeatureCategory category, int order, int& activations)
            : category_(std::move(category))
            , order_(order)
            , activations_(activations)
        {
        }
        void setup(systems::feature::FeatureRegistrar& reg, systems::feature::FeatureContext&) override
        {
            if (!category_.id.empty())
                reg.navigation().addCategory(category_);
            reg.navigation().setOrder(order_);
            reg.addParameter({ ArgTypeEnum::Float, "目标尺寸", "1", "" });
        }
        void activate(systems::feature::FeatureContext&) override { ++activations_; }
        std::any execute(systems::feature::FeatureContext&) override { return std::string("请选择目标组件"); }

    private:
        systems::feature::FeatureCategory category_;
        int order_;
        int& activations_;
    };
    ModelLayer model;
    core::EventBus events;
    systems::feature::FeatureSystem system(model, events);
    QTaskStatus status;
    systems::feature::QFeatureSystemAdaptor adaptor(system);
    auto subscription = events.subscribe<systems::feature::ParameterChangedEvent>([&](const auto& event) {
        adaptor.notifyParameterChanged(event.feature, event.param_index, event.value);
    });
    QQmlEngine engine;
    static const int theme_type = qmlRegisterSingletonType(QUrl("qrc:/navigation-ui/Theme.qml"), "DynamicNavigationUi", 1, 0, "Theme");
    engine.rootContext()->setContextProperty("Theme", engine.singletonInstance<QObject*>(theme_type));
    QQmlComponent component(&engine, QUrl("qrc:/navigation-ui/AppToolbar.qml"));
    REQUIRE(component.isReady());
    QQmlEngine::setObjectOwnership(&adaptor, QQmlEngine::CppOwnership);
    auto provider = engine.newQObject(&adaptor);
    auto session = createSession(engine, provider);
    std::unique_ptr<QObject> toolbar(component.createWithInitialProperties({ { "meshFeatureSystem", QVariant::fromValue(provider) } }));
    auto* item = qobject_cast<QQuickItem*>(toolbar.get());
    REQUIRE(item);
    QQuickWindow window;
    window.resize(900, 700);
    item->setParentItem(window.contentItem());
    item->setSize(QSizeF(900, 180));
    window.show();
    QQmlComponent sidebar_component(&engine, QUrl("qrc:/navigation-ui/SideBar.qml"));
    REQUIRE(sidebar_component.isReady());
    std::unique_ptr<QObject> sidebar(sidebar_component.createWithInitialProperties({ { "session", QVariant::fromValue(session.get()) } }));
    auto* sidebar_item = qobject_cast<QQuickItem*>(sidebar.get());
    REQUIRE(sidebar_item);
    sidebar_item->setParentItem(window.contentItem());
    sidebar_item->setPosition(QPointF(0, 180));
    sidebar_item->setSize(QSizeF(450, 500));
    REQUIRE(toolbar->setProperty("activeCategory", 2));
    const auto settle = [] {
        QEventLoop loop;
        QTimer::singleShot(60, &loop, &QEventLoop::quit);
        loop.exec();
    };
    auto* app = engine.singletonInstance<QObject*>("app.core", "App");
    REQUIRE(app);
    CHECK_FALSE(findItem(item, "algorithmCategory_polyhedral"));
    settle();
    // 没有安装任何算法时，四个基础入口也必须可见、可点击。
    CHECK(adaptor.getNavigationCategories().size() == 4);
    for (const auto& entry : adaptor.getNavigationCategories()) {
        const auto category = entry.toMap();
        auto* category_button = findItem(item, "algorithmCategory_" + category.value("id").toString());
        REQUIRE(category_button);
        CHECK(engine.newQObject(category_button).property("icon").property("source").toString() == category.value("icon").toString());
    }
    auto* hexahedron_button = findItem(item, "algorithmCategory_hexahedron");
    REQUIRE(hexahedron_button);
    clickItem(window, hexahedron_button);
    settle();
    CHECK(session->property("panelTitle").toString() == "操作面板-六面体网格生成");
    CHECK(session->property("meshAlgorithms").value<QJSValue>().property("length").toInt() == 0);
    CHECK(app->property("activeOperation").value<QJSValue>().property("info").isNull());
    auto* empty_label = findAction(sidebar_item, "暂无可用算法");
    REQUIRE(empty_label);
    CHECK(empty_label->isVisible());
    auto* buttons = findItem(sidebar_item, "operationButtons");
    REQUIRE(buttons);
    bool found_execute = false;
    for (auto* button : buttons->childItems()) {
        if (button->property("text").toString() == "执行") {
            found_execute = true;
            CHECK_FALSE(button->isEnabled());
        }
    }
    CHECK(found_execute);
    // 后装算法直接出现在既有分类；最后一个提供者卸载后保留入口并清空参数。
    int activations = 0;
    const systems::feature::HandlerMetaData hexahedron { .name = "hexahedron-test" };
    REQUIRE(system.registerHandler(hexahedron, systems::feature::FeatureSystem::SystemHandlerPtr { std::make_unique<CategoryHandler>(systems::feature::FeatureCategory { "hexahedron", "六面体网格生成", "", 40 }, 0, activations).release() }));
    settle();
    CHECK(session->property("meshAlgorithms").value<QJSValue>().property("length").toInt() == 1);
    CHECK(app->property("activeOperation").value<QJSValue>().property("info").property("name").toString() == "hexahedron-test");
    system.unregisterHandler(hexahedron);
    settle();
    REQUIRE(findItem(item, "algorithmCategory_hexahedron"));
    CHECK(session->property("panelTitle").toString() == "操作面板-六面体网格生成");
    CHECK(session->property("meshAlgorithms").value<QJSValue>().property("length").toInt() == 0);
    CHECK(app->property("activeOperation").value<QJSValue>().property("info").isNull());
    CHECK(session->property("parameterModel").value<QObject*>()->property("values").value<QJSValue>().property("length").toInt() == 0);
    const systems::feature::FeatureCategory category { "polyhedral", "多面体网格生成", "qrc:/images/toolbar/Mesh/hexa-meshing.svg", 5 };
    const auto install = [&](const auto& metadata, const auto& declaration, int order) {
        return system.registerHandler(metadata, systems::feature::FeatureSystem::SystemHandlerPtr { std::make_unique<CategoryHandler>(declaration, order, activations).release() });
    };
    const systems::feature::HandlerMetaData first { .name = "first", .display_name = "first" };
    const systems::feature::HandlerMetaData second { .name = "second", .display_name = "second" };
    REQUIRE(install(first, category, 10));
    REQUIRE(install(second, category, 20));
    settle();
    CHECK(adaptor.getNavigationCategories().size() == 5);
    auto* button = findItem(item, "algorithmCategory_polyhedral");
    REQUIRE(button);
    CHECK(button->property("text").toString() == "多面体网格生成");
    CHECK(engine.newQObject(button).property("icon").property("source").toString() == QString::fromStdString(category.icon));
    clickItem(window, button);
    settle();
    CHECK(session->property("meshAlgorithms").value<QJSValue>().property("length").toInt() == 2);
    CHECK(session->property("panelTitle").toString() == "操作面板-多面体网格生成");
    auto operation = app->property("activeOperation").value<QJSValue>();
    auto execute = operation.property("execute");
    REQUIRE(execute.isCallable());
    CHECK(execute.callWithInstance(operation).toString() == "请选择目标组件");
    auto* parameters = session->property("parameterModel").value<QObject*>();
    REQUIRE(parameters);
    auto* selector = findItem(sidebar_item, "meshAlgorithmSelector");
    REQUIRE(selector);
    CHECK(selector->property("count").toInt() == 2);
    clickItem(window, selector);
    pressKey(window, Qt::Key_Down);
    pressKey(window, Qt::Key_Return);
    settle();
    CHECK(app->property("activeOperation").value<QJSValue>().property("info").property("name").toString() == "second");
    clickItem(window, selector);
    pressKey(window, Qt::Key_Up);
    pressKey(window, Qt::Key_Return);
    settle();
    CHECK(app->property("activeOperation").value<QJSValue>().property("info").property("name").toString() == "first");
    auto* numeric = findItem(sidebar_item, "parameterControl_0");
    REQUIRE(numeric);
    clickItem(window, numeric);
    pressKey(window, Qt::Key_A, Qt::ControlModifier);
    pressKey(window, Qt::Key_9, Qt::NoModifier, "9");
    pressKey(window, Qt::Key_Tab);
    settle();
    CHECK(parameters->property("values").value<QJSValue>().property(0).toNumber() == 9.0);
    auto* selection = app->property("selection").value<QObject*>();
    REQUIRE(selection);
    selection->setProperty("listeningSelectorIndex", 0);
    // 不相关注册和分类显示名称刷新都不能重置当前参数或选择。
    const systems::feature::HandlerMetaData unrelated { .name = "unrelated" };
    const int before_refresh = activations;
    REQUIRE(install(unrelated, systems::feature::FeatureCategory {}, 0));
    settle();
    CHECK(activations == before_refresh);
    auto renamed_category = category;
    renamed_category.title = "多面体划分";
    REQUIRE(install(first, renamed_category, 10));
    settle();
    CHECK(parameters->property("values").value<QJSValue>().property(0).toNumber() == 9.0);
    CHECK(selection->property("listeningSelectorIndex").toInt() == 0);
    CHECK(session->property("panelTitle").toString() == "操作面板-多面体划分");
    CHECK(activations == before_refresh + 1);
    // 当前算法退出后改选仍在的提供者；最后一个退出后清理整个操作。
    system.unregisterHandler(first);
    settle();
    REQUIRE(findItem(item, "algorithmCategory_polyhedral"));
    CHECK(app->property("activeOperation").value<QJSValue>().property("info").property("name").toString() == "second");
    CHECK(parameters->property("values").value<QJSValue>().property(0).toNumber() == 1.0);
    CHECK(selection->property("listeningSelectorIndex").toInt() == -1);
    selection->setProperty("listeningSelectorIndex", 0);
    system.unregisterHandler(second);
    settle();
    CHECK_FALSE(findItem(item, "algorithmCategory_polyhedral"));
    CHECK(app->property("activeOperation").isNull());
    CHECK_FALSE(session->property("meshGeneration").toBool());
    CHECK(parameters->property("values").value<QJSValue>().property("length").toInt() == 0);
    CHECK(selection->property("listeningSelectorIndex").toInt() == -1);
    CHECK(adaptor.getFeaturesInfo().size() == 1);
    system.unregisterHandler(unrelated);
    QModelManager::argv0 = {};
}
