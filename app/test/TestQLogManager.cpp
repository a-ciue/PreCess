/**
 * @file TestQLogManager.cpp
 * @brief QLogManager Qt/QML 消息桥接测试
 *
 * 验证 qWarning/qDebug 等 Qt 消息与 spdlog 消息一样进入日志面板，统一为
 * [时间] [QML] [级别] 消息格式，并保留分级配色与 [QML] 来源标签色。
 */
#include "QLogManager.h"

#include <QColor>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QRegularExpression>
#include <QTest>
#include <QVariantMap>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

//! @brief 按子串查找日志面板中的消息，返回匹配到的 HTML 行
QString findMessage(const QStringList& messages, const QString& text)
{
    for (const QString& message : messages) {
        if (message.contains(text))
            return message;
    }
    return QString();
}

//! @brief 进程内唯一的离屏 GUI 应用，不创建窗口或图形上下文。
QGuiApplication& ensureApplication()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    static int argc = 1;
    static char app_name[] = "TestQLogManager";
    static char* argv[] = { app_name, nullptr };
    static QGuiApplication app(argc, argv);
    return app;
}

} // namespace

TEST_CASE("QLogManager bridges Qt and QML messages into the log panel")
{
    (void)ensureApplication();
    QLogManager::initialize();

    qWarning("qlog-test-warning");
    const QLoggingCategory qml_category("qml");
    qCWarning(qml_category) << "qlog-test-qml-warning";
    qDebug("qlog-test-debug");
    spdlog::warn("qlog-test-spdlog");
    QCoreApplication::processEvents();

    auto* mgr = QLogManager::instance();
    REQUIRE(mgr != nullptr);
    const QStringList messages = mgr->messages();

    // 统一格式：[时间] [QML] [级别] 消息；[QML] 标签用来源蓝，正文按级别着色。
    // spdlog %e 固定 3 位毫秒，精度/格式变化属契约变更，应让用例显式失败
    const QRegularExpression timestamp(QStringLiteral("\\[\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\.\\d{3}\\]"));

    const QString warning = findMessage(messages, "qlog-test-warning");
    REQUIRE(warning.startsWith(QStringLiteral("<span style='color:#e65100; white-space:pre;'>")));
    REQUIRE(warning.contains(timestamp));
    REQUIRE(warning.contains(
        QStringLiteral("<span style='color:#1976d2; white-space:pre;'>[QML]</span> [warning] ")));
    REQUIRE(warning.endsWith(QStringLiteral("qlog-test-warning</span>")));

    const QString qml_warning = findMessage(messages, "qlog-test-qml-warning");
    REQUIRE_FALSE(qml_warning.isEmpty());
    REQUIRE(qml_warning.contains("[QML]"));
    REQUIRE(qml_warning.contains("[warning]"));
    REQUIRE(qml_warning.contains("#1976d2"));
    REQUIRE(qml_warning.contains("#e65100"));

    const QString debug = findMessage(messages, "qlog-test-debug");
    REQUIRE_FALSE(debug.isEmpty());
    REQUIRE(debug.contains("[QML]"));
    REQUIRE(debug.contains("[debug]"));
    REQUIRE(debug.contains("#616161")); // DEBUG 级别色

    const QString spdlog_message = findMessage(messages, "qlog-test-spdlog");
    REQUIRE_FALSE(spdlog_message.isEmpty());
    REQUIRE(spdlog_message.contains(timestamp));
    REQUIRE_FALSE(spdlog_message.contains("[QML]"));
    REQUIRE(spdlog_message.contains("#e65100"));

    // appendMessage 对原始文本统一转义，避免 HTML 注入
    mgr->appendMessage(QStringLiteral("INFO"), QStringLiteral("<b>qlog-test-raw</b>"));
    const QString escaped = findMessage(mgr->messages(), "qlog-test-raw");
    REQUIRE_FALSE(escaped.isEmpty());
    REQUIRE(escaped.contains(QStringLiteral("&lt;b&gt;qlog-test-raw&lt;/b&gt;")));

    // 正文包含与来源标签相同的子串时，仅着色紧随时间戳的 header 段
    qWarning("[QML] qlog-test-body");
    QCoreApplication::processEvents();
    const QString body_line = findMessage(mgr->messages(), "qlog-test-body");
    REQUIRE_FALSE(body_line.isEmpty());
    REQUIRE(body_line.count(QStringLiteral("#1976d2")) == 1);
    REQUIRE(body_line.contains(QStringLiteral("[warning] [QML] qlog-test-body")));

    // 同文件名的导入跟踪可先于绑定错误出现，定位错误时不能误选这条 debug 消息。
    qDebug("qrc:/TestQLogManager.qml: qlog-test-import-trace");
    // QML 运行时报错（绑定表达式引用未定义标识符）经 qWarning 汇入日志面板
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(
        "import QtQml\nQtObject { property int broken: no_such_identifier }",
        QUrl(QStringLiteral("qrc:/TestQLogManager.qml")));
    const std::unique_ptr<QObject> qml_object(component.create());
    QCoreApplication::processEvents();

    REQUIRE(qml_object != nullptr);
    const QString qml_error = findMessage(mgr->messages(), "ReferenceError: no_such_identifier");
    REQUIRE_FALSE(qml_error.isEmpty());
    REQUIRE(qml_error.contains("[QML]"));
    REQUIRE(qml_error.contains("[warning]"));
    REQUIRE(qml_error.contains("#1976d2"));
    REQUIRE(qml_error.contains("TestQLogManager.qml"));
}

TEST_CASE("Output log follows new records only while scrolled to the bottom", "[QML][log][no-window]")
{
    ensureApplication();
    QQuickStyle::setStyle("Fusion");
    QLogManager manager;
    for (int i = 0; i < 120; ++i)
        manager.appendMessage("INFO", QStringLiteral("History record %1").arg(i));

    QQmlEngine engine;
    engine.rootContext()->setContextProperty("QLogManager", &manager);
    engine.rootContext()->setContextProperty("Theme", QVariantMap { { "textPrimary", QColor("#263E48") }, { "monoFamily", QStringLiteral("Consolas") }, { "fontSizeCaption", 12 } });
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/log-ui/OutputLog.qml")));
    INFO(component.errorString().toStdString());
    REQUIRE(component.isReady());
    const std::unique_ptr<QObject> object(component.create());
    auto* root = qobject_cast<QQuickItem*>(object.get());
    REQUIRE(root);
    root->setSize(QSizeF(420, 180));

    // 仅执行布局，不创建窗口或图形上下文。
    const auto polish = [&] {
        root->ensurePolished();
        for (auto* item : root->findChildren<QQuickItem*>())
            item->ensurePolished();
    };
    polish();
    auto* scroll = root->findChild<QQuickItem*>(QStringLiteral("outputLogScroll"));
    REQUIRE(scroll);
    auto* viewport = scroll->property("contentItem").value<QQuickItem*>();
    REQUIRE(viewport);
    REQUIRE(QTest::qWaitFor([&] {
        polish();
        return viewport->height() > 0 && viewport->property("contentHeight").toReal() > viewport->height();
    }));
    const auto bottom = [&] {
        return viewport->property("originY").toReal()
            + std::max<qreal>(0, viewport->property("contentHeight").toReal() - viewport->height());
    };
    const auto at_bottom = [&] {
        polish();
        return std::abs(viewport->property("contentY").toReal() - bottom()) < 1;
    };
    REQUIRE(QTest::qWaitFor(at_bottom));

    // 历史文字中的光标位置不能把已滚到底部的日志拉回中间。
    auto* text_area = root->findChild<QQuickItem*>(QStringLiteral("outputLogText"));
    REQUIRE(text_area);
    text_area->setProperty("cursorPosition", text_area->property("length").toInt() / 3);
    viewport->setProperty("contentY", bottom());
    REQUIRE(at_bottom());
    const auto old_height = viewport->property("contentHeight").toReal();
    manager.appendMessage("WARN", QStringLiteral("New record with wrapping text ").repeated(20));
    REQUIRE(QTest::qWaitFor([&] {
        polish();
        return viewport->property("contentHeight").toReal() > old_height;
    }));
    REQUIRE(QTest::qWaitFor(at_bottom));

    // 向上阅读历史时，新记录不应抢走阅读位置。
    const auto reading_y = bottom() / 3;
    viewport->setProperty("contentY", reading_y);
    manager.appendMessage("INFO", "Record while reading history");
    QCoreApplication::processEvents();
    polish();
    CHECK(viewport->property("contentY").toReal() == Catch::Approx(reading_y).margin(1));

    // 回到底部后，连续追加及换行仍保持最新记录可见。
    viewport->setProperty("contentY", bottom());
    for (int i = 0; i < 20; ++i)
        manager.appendMessage("INFO", QStringLiteral("Burst record %1 ").arg(i).repeated(15));
    REQUIRE(QTest::qWaitFor(at_bottom));
}
