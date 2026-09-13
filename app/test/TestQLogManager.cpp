/**
 * @file TestQLogManager.cpp
 * @brief QLogManager Qt/QML 消息桥接测试
 *
 * 验证 qWarning/qDebug 等 Qt 消息与 spdlog 消息一样进入日志面板，统一为
 * [时间] [QML] [级别] 消息格式，并保留分级配色与 [QML] 来源标签色。
 */
#include "QLogManager.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QRegularExpression>

#include <catch2/catch_test_macros.hpp>
#include <spdlog/spdlog.h>

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

//! @brief 进程内唯一的 QCoreApplication：Catch2 单进程不可重复构造，静态化以兼容后续用例
QCoreApplication& ensureApplication()
{
    static int argc = 1;
    static char app_name[] = "TestQLogManager";
    static char* argv[] = { app_name, nullptr };
    static QCoreApplication app(argc, argv);
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

    // QML 运行时报错（绑定表达式引用未定义标识符）经 qWarning 汇入日志面板
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(
        "import QtQml\nQtObject { property int broken: no_such_identifier }",
        QUrl(QStringLiteral("qrc:/TestQLogManager.qml")));
    const std::unique_ptr<QObject> qml_object(component.create());
    QCoreApplication::processEvents();

    REQUIRE(qml_object != nullptr);
    const QString qml_error = findMessage(mgr->messages(), "TestQLogManager.qml");
    REQUIRE_FALSE(qml_error.isEmpty());
    REQUIRE(qml_error.contains("[QML]"));
    REQUIRE(qml_error.contains("[warning]"));
    REQUIRE(qml_error.contains("#1976d2"));
    REQUIRE(qml_error.contains("no_such_identifier"));
}
