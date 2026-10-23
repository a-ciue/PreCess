/**
 * @file TestThemeQml.cpp
 * @brief Theme.qml 契约：角色键完整、壁纸失败仅运行时降级面色
 */
#include "core/ThemePalette.h"

#include <QColor>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QSettings>
#include <QStandardPaths>
#include <QVariantMap>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {

QGuiApplication& testApplication()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    static int argc = 1;
    static char name[] = "TestThemeQml";
    static char* argv[] = { name, nullptr };
    static QGuiApplication app(argc, argv);
    return app;
}

QColor colorOf(const QVariantMap& colors, const char* key)
{
    return colors.value(QLatin1String(key)).value<QColor>();
}

}

TEST_CASE("Theme.qml exposes complete role maps and downgrades failed wallpaper", "[Theme][QML]")
{
    testApplication();
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("PreCessThemeTest"));
    QCoreApplication::setApplicationName(QStringLiteral("PreCessThemeTest"));
    QSettings().clear();

    QQmlEngine engine;
    static const int theme_type = qmlRegisterSingletonType(
        QUrl(QStringLiteral("qrc:/theme-qml/Theme.qml")), "ThemeQmlTest", 1, 0, "Theme");
    auto* theme = engine.singletonInstance<QObject*>(theme_type);
    REQUIRE(theme);

    REQUIRE(QMetaObject::invokeMethod(theme, "setWallpaper", Q_ARG(QVariant, QStringLiteral("test.png"))));
    QCoreApplication::processEvents();

    const QVariantMap control = theme->property("controlPalette").value<QVariantMap>();
    const QVariantMap popup = theme->property("popupPalette").value<QVariantMap>();
    const QVariantMap dock = theme->property("dockColors").value<QVariantMap>();

    // Theme.qml 与 ThemePalette.h 的角色键契约
    REQUIRE(app::core::missingThemeRoles(control).isEmpty());
    REQUIRE(app::core::missingThemeRoles(popup).isEmpty());

    const QStringList dock_keys {
        QStringLiteral("windowBackground"), QStringLiteral("surface"), QStringLiteral("surfaceAlt"),
        QStringLiteral("panelHeaderSurface"), QStringLiteral("menuSurface"), QStringLiteral("wallpaperTint"),
        QStringLiteral("border"), QStringLiteral("borderStrong"), QStringLiteral("textPrimary"),
        QStringLiteral("textDisabled"), QStringLiteral("textOnPrimary"), QStringLiteral("primary"),
        QStringLiteral("danger"), QStringLiteral("scrollBarHover"), QStringLiteral("chromeIcon"),
        QStringLiteral("chromeHover"), QStringLiteral("dividerHover"), QStringLiteral("dropAccent"),
        QStringLiteral("dropZoneFill"), QStringLiteral("dropZoneBorder"), QStringLiteral("dropZoneMarker"),
        QStringLiteral("dropZoneActiveFore")
    };
    for (const QString& key : dock_keys)
        REQUIRE(dock.contains(key));

    // 壁纸可用：面色半透明
    REQUIRE(colorOf(control, "base").alphaF() == Catch::Approx(0.72).epsilon(0.001));
    REQUIRE(colorOf(control, "alternateBase").alphaF() == Catch::Approx(0.62).epsilon(0.001));

    // 同 id 重试：身份不变，但图像源通过运行时令牌变化
    const QUrl original_source = theme->property("wallpaperSource").toUrl();
    REQUIRE(QMetaObject::invokeMethod(theme, "retryWallpaper"));
    REQUIRE(theme->property("wallpaperReloadToken").toInt() == 1);
    REQUIRE(theme->property("wallpaper").toString() == QStringLiteral("test.png"));
    REQUIRE(theme->property("wallpaperSource").toUrl() != original_source);

    // 加载失败：保留选择，但面色临时回退为不透明
    theme->setProperty("wallpaperReady", false);
    QCoreApplication::processEvents();
    REQUIRE(theme->property("hasWallpaper").toBool());
    REQUIRE(colorOf(theme->property("controlPalette").value<QVariantMap>(), "base").alphaF()
        == Catch::Approx(1.0));
    REQUIRE(colorOf(theme->property("controlPalette").value<QVariantMap>(), "alternateBase").alphaF()
        == Catch::Approx(1.0));

    theme->setProperty("wallpaperReady", true);
    REQUIRE(QMetaObject::invokeMethod(theme, "setWallpaper", Q_ARG(QVariant, QString())));
    QSettings().clear();
}
