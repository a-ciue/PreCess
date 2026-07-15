#include "QLogManager.h"

#include <QMetaObject>

#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

template <typename Mutex>
class QtLogSink : public spdlog::sinks::base_sink<Mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        QString text = QString::fromUtf8(formatted.data(), static_cast<int>(formatted.size()));
        if (text.endsWith('\n'))
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

        auto* mgr = QLogManager::instance();
        if (!mgr)
            return;

        QMetaObject::invokeMethod(mgr, "appendMessage", Qt::QueuedConnection,
            Q_ARG(QString, levelStr),
            Q_ARG(QString, text));
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
}

QLogManager* QLogManager::instance()
{
    return self_.get();
}

QStringList QLogManager::messages() const
{
    return messages_;
}

void QLogManager::appendMessage(const QString& level, const QString& message)
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

    QString html = QStringLiteral(
        "<span style='color:%1; white-space:pre;'>%2</span>")
                       .arg(color, message.toHtmlEscaped());

    messages_.append(html);
    emit newMessage(level, html);
    emit messagesChanged();
}
