/**
 * @file TestPreferencesWindow.cpp
 * @brief 偏好设置面板运行时 smoke：实际 QML 加载、紧凑布局与壁纸动作按钮
 */
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickStyle>
#include <QSettings>
#include <QStandardPaths>
#include <catch2/catch_test_macros.hpp>
#include <memory>

Q_IMPORT_QML_PLUGIN(app_corePlugin)
Q_IMPORT_QML_PLUGIN(app_modelPlugin)
Q_IMPORT_QML_PLUGIN(app_model_systemsPlugin)

namespace {

class StubPluginManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList pluginNames READ pluginNames CONSTANT)
    Q_PROPERTY(bool canLoadExternalPlugins READ canLoadExternalPlugins CONSTANT)

public:
    explicit StubPluginManager(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    QStringList pluginNames() const { return {}; }
    bool canLoadExternalPlugins() const { return true; }
};

class StubModelManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* systemPluginManager READ systemPluginManager CONSTANT)

public:
    explicit StubModelManager(QObject* parent = nullptr)
        : QObject(parent)
        , plugin_manager_(new StubPluginManager(this))
    {
    }

    QObject* systemPluginManager() const { return plugin_manager_; }

private:
    StubPluginManager* plugin_manager_;
};

QGuiApplication& testApplication()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    static int argc = 1;
    static char name[] = "TestPreferencesWindow";
    static char* argv[] = { name, nullptr };
    static QGuiApplication app(argc, argv);
    return app;
}

QQuickItem* findItem(QQuickItem* root, const QString& name)
{
    return root ? root->findChild<QQuickItem*>(name) : nullptr;
}

QStringList& qtMessages()
{
    static QStringList messages;
    return messages;
}

void captureQtMessage(QtMsgType, const QMessageLogContext&, const QString& message)
{
    qtMessages().append(message);
}

}

TEST_CASE("Preferences window loads and follows compact and wide layouts", "[QML][Preferences]")
{
    testApplication();
    QQuickStyle::setStyle("Fusion");
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("PreCessPreferencesTest"));
    QCoreApplication::setApplicationName(QStringLiteral("PreCessPreferencesTest"));
    QSettings().clear();

    QQmlEngine engine;
    const QtMessageHandler previous_handler = qInstallMessageHandler(captureQtMessage);
    static const int theme_type = qmlRegisterSingletonType(
        QUrl(QStringLiteral("qrc:/preferences-ui/Theme.qml")), "PreferencesUi", 1, 0, "Theme");
    auto* theme = engine.singletonInstance<QObject*>(theme_type);
    REQUIRE(theme);
    REQUIRE(QMetaObject::invokeMethod(theme, "setWallpaper", Q_ARG(QVariant, QString())));

    // 偏好页只依赖这些查询；用桩隔离 QModelManager 的动态插件扫描。
    StubModelManager model_manager;
    engine.rootContext()->setContextProperty("Theme", theme);
    engine.rootContext()->setContextProperty("QModelManager", &model_manager);

    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/preferences-ui/PreferencesWindow.qml")));
    INFO(component.errorString().toStdString());
    INFO(qtMessages().join(QLatin1Char('\n')).toStdString());
    REQUIRE(component.isReady());
    std::unique_ptr<QObject> object(component.create());
    REQUIRE(object);

    auto* window = qobject_cast<QQuickItem*>(object.get());
    REQUIRE(window);
    window->setSize(QSizeF(360, 420));
    QCoreApplication::processEvents();

    REQUIRE(window->property("compact").toBool());
    auto* mode_switch = findItem(window, QStringLiteral("themeModeSwitch"));
    REQUIRE(mode_switch);
    REQUIRE(mode_switch->property("compact").toBool());
    REQUIRE(mode_switch->property("implicitWidth").toReal() == 116);

    auto* add_button = findItem(window, QStringLiteral("wallpaperActionButton"));
    auto* clear_button = findItem(window, QStringLiteral("clearWallpaperButton"));
    REQUIRE(add_button);
    REQUIRE(clear_button);
    REQUIRE(add_button->property("text").toString() == QStringLiteral("添加图片"));
    REQUIRE_FALSE(clear_button->isVisible());

    // 异步导入完成信号：更新 Theme、按钮状态与短反馈
    auto* wallpaper = engine.singletonInstance<QObject*>("app.core", "Wallpaper");
    REQUIRE(wallpaper);
    REQUIRE(QMetaObject::invokeMethod(wallpaper, "importFinished", Q_ARG(QString, QStringLiteral("test.png"))));
    QCoreApplication::processEvents();
    REQUIRE(theme->property("wallpaper").toString() == QStringLiteral("test.png"));
    REQUIRE(add_button->property("text").toString() == QStringLiteral("更换图片"));
    REQUIRE(clear_button->isVisible());

    // 重复导入同一 id：不重复写 Theme，给出短暂反馈
    REQUIRE(QMetaObject::invokeMethod(wallpaper, "importFinished", Q_ARG(QString, QStringLiteral("test.png"))));
    QCoreApplication::processEvents();
    REQUIRE(window->property("wallpaperStatus").toString() == QStringLiteral("当前壁纸已导入"));

    window->setSize(QSizeF(650, 420));
    QCoreApplication::processEvents();
    REQUIRE_FALSE(window->property("compact").toBool());
    REQUIRE_FALSE(mode_switch->property("compact").toBool());
    REQUIRE(mode_switch->property("implicitWidth").toReal() == 176);

    REQUIRE(QMetaObject::invokeMethod(theme, "setWallpaper", Q_ARG(QVariant, QString())));
    const QString qml_messages = qtMessages().join(QLatin1Char('\n'));
    INFO(qml_messages.toStdString());
    CHECK_FALSE(qml_messages.contains(QStringLiteral("is not a function")));
    CHECK_FALSE(qml_messages.contains(QStringLiteral("TypeError")));

    QSettings().clear();
    qInstallMessageHandler(previous_handler);
}

#include "TestPreferencesWindow.moc"
