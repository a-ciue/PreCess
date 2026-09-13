#include "QLogManager.h"

#include <QMetaObject>
#include <QRegularExpression>
#include <QtLogging>

#include <cstdio>
#include <cstring>

#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

namespace {

//! @brief QML/Qt 消息专用 logger 名与 [QML] 来源标签的单一来源
constexpr char kQmlLoggerName[] = "QML";

//! @brief 来源标签色（与 JavaScriptConsole 主题蓝一致）
constexpr char kSourceColor[] = "#1976d2";

//! @brief 默认 spdlog 格式行首的时间戳（[%Y-%m-%d %H:%M:%S.%e]），用于定位来源段
const QRegularExpression& timestampEndRegex()
{
    static const QRegularExpression rx(
        QStringLiteral("^\\[\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2}\\.\\d{3}\\]"));
    return rx;
}

//! @brief 安装本处理器时返回的上一级处理器，用于保留 Qt/宿主的既有输出行为
QtMessageHandler g_previous_handler = nullptr;

//! @brief QML/Qt 消息专用 logger：与主日志共用 sink、格式一致；级别恒为 trace，
//!        避免 QML 调试信息被 SPDLOG_LEVEL 默认的 info 档过滤
std::shared_ptr<spdlog::logger> g_qt_logger;

//! @brief 将 Qt/QML 消息桥接到日志面板
//!
//! QML 运行时报错、console.log/warn/error、属性绑定警告与 Qt 内部警告都经
//! qDebug/qWarning 输出，统一映射为 spdlog 级别后由 "QML" logger 格式化，与主日志
//! 共用时间戳/级别/来源格式；同时调用上一级处理器（Qt 6.8 起无自定义处理器时返回
//! 默认处理器，旧版本可能为 nullptr），保留终端与调试器的既有输出；无上一级处理器
//! 时自行补写 stderr。
void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    // 保留既有处理器（如调试器/宿主安装的处理器）的行为
    if (g_previous_handler) {
        g_previous_handler(type, context, message);
    } else {
        QString formatted = qFormatLogMessage(type, context, message);
        if (formatted.endsWith(QLatin1Char('\n')))
            formatted.chop(1);
        std::fprintf(stderr, "%s\n", formatted.toLocal8Bit().constData());
        std::fflush(stderr);
    }

    spdlog::level::level_enum level = spdlog::level::debug;
    switch (type) {
    case QtDebugMsg:
        level = spdlog::level::debug;
        break;
    case QtInfoMsg:
        level = spdlog::level::info;
        break;
    case QtWarningMsg:
        level = spdlog::level::warn;
        break;
    case QtCriticalMsg:
        level = spdlog::level::err;
        break;
    case QtFatalMsg:
        level = spdlog::level::critical;
        break;
    }

    // 非默认 category（如 qt.qpa.*）保留来源前缀；qml 已由 [QML] 标签标识
    QString text = message;
    if (context.category && std::strcmp(context.category, "default") != 0
        && std::strcmp(context.category, "qml") != 0) {
        text = QStringLiteral("[%1] %2").arg(QString::fromUtf8(context.category), message);
    }

    // 防御：初始化完成前/退出期 g_qt_logger 可能为空（正常路径下与 instance() 同生共死）
    if (QLogManager::instance() && g_qt_logger)
        g_qt_logger->log(level, "{}", text.toStdString());
}

} // namespace

template <typename Mutex>
class QtLogSink : public spdlog::sinks::base_sink<Mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        QString text = QString::fromUtf8(formatted.data(), static_cast<int>(formatted.size()));
        // 去掉格式化器追加的平台行尾：spdlog 默认 EOL 为 SPDLOG_EOL（Windows 为 "\r\n"）
        if (text.endsWith(QLatin1String("\r\n")))
            text.chop(2);
        else if (text.endsWith(QLatin1Char('\n')))
            text.chop(1);

        QString levelStr;
        switch (msg.level) {
        case spdlog::level::trace:
            levelStr = QStringLiteral("TRACE");
            break;
        case spdlog::level::debug:
            levelStr = QStringLiteral("DEBUG");
            break;
        case spdlog::level::info:
            levelStr = QStringLiteral("INFO");
            break;
        case spdlog::level::warn:
            levelStr = QStringLiteral("WARN");
            break;
        case spdlog::level::err:
            levelStr = QStringLiteral("ERROR");
            break;
        case spdlog::level::critical:
            levelStr = QStringLiteral("FATAL");
            break;
        default:
            levelStr = QStringLiteral("OFF");
            break;
        }

        // 来源名交由 appendMessage 转义并着色，此处仅标识来源
        const QString source = (msg.logger_name == spdlog::string_view_t(kQmlLoggerName))
            ? QString::fromLatin1(kQmlLoggerName)
            : QString();

        auto* mgr = QLogManager::instance();
        if (!mgr)
            return;

        QMetaObject::invokeMethod(mgr, "appendMessage", Qt::QueuedConnection,
            Q_ARG(QString, levelStr),
            Q_ARG(QString, text),
            Q_ARG(QString, source));
    }

    void flush_() override { }
};

using QtLogSink_mt = QtLogSink<std::mutex>;

std::unique_ptr<QLogManager> QLogManager::self_;

QLogManager::QLogManager(QObject* parent)
    : QObject(parent)
{
}

void QLogManager::initialize()
{
    if (self_)
        return;

    self_ = std::make_unique<QLogManager>();

    auto sink = std::make_shared<QtLogSink_mt>();
    auto logger = std::make_shared<spdlog::logger>("PreCess", sink);
    logger->set_level(spdlog::get_level());
    spdlog::set_default_logger(logger);

    // QML/Qt 消息专用 logger：与主日志共用 sink 与格式，级别 trace 保证调试信息不被过滤
    g_qt_logger = std::make_shared<spdlog::logger>(kQmlLoggerName, sink);
    g_qt_logger->set_level(spdlog::level::trace);

    // 接管 Qt/QML 消息（QML 报错、console.*、绑定警告等），汇入同一日志面板
    g_previous_handler = qInstallMessageHandler(qtMessageHandler);
}

QLogManager* QLogManager::instance()
{
    return self_.get();
}

QStringList QLogManager::messages() const
{
    return messages_;
}

void QLogManager::appendMessage(const QString& level, const QString& message, const QString& source)
{
    if (messages_.size() >= MaxMessages)
        messages_.removeFirst();

    QString color;
    if (level == QLatin1String("ERROR") || level == QLatin1String("FATAL"))
        color = QStringLiteral("#d32f2f");
    else if (level == QLatin1String("WARN"))
        color = QStringLiteral("#e65100");
    else if (level == QLatin1String("INFO"))
        color = QStringLiteral("#2e7d32");
    else if (level == QLatin1String("DEBUG"))
        color = QStringLiteral("#616161");
    else if (level == QLatin1String("TRACE"))
        color = QStringLiteral("#9e9e9e");
    else
        color = QStringLiteral("#333333");

    // 原始文本统一转义；来源标签单独着色，正文按级别着色。
    // 仅着色紧随时间戳的来源段（"[时间] [来源] [级别] …"），避免误改正文中的同名子串
    QString html = message.toHtmlEscaped();
    if (!source.isEmpty()) {
        const QString tag = QStringLiteral("[%1]").arg(source.toHtmlEscaped());
        const qsizetype tag_pos = html.indexOf(tag);
        const QRegularExpressionMatch match = timestampEndRegex().match(html);
        if (tag_pos >= 0 && match.hasMatch() && tag_pos == match.capturedEnd() + 1) {
            html.replace(tag_pos, tag.size(),
                QStringLiteral("<span style='color:%1; white-space:pre;'>%2</span>")
                    .arg(QString::fromLatin1(kSourceColor), tag));
        }
    }

    html = QStringLiteral("<span style='color:%1; white-space:pre;'>%2</span>")
               .arg(color, html);

    messages_.append(html);
    emit newMessage(level, html);
    emit messagesChanged();
}
