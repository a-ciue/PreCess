#pragma once

#include <QObject>
#include <QStringList>
#include <memory>

class QLogManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList messages READ messages NOTIFY messagesChanged)

public:
    explicit QLogManager(QObject* parent = nullptr);
    static void initialize();
    static QLogManager* instance();

    QStringList messages() const;

    //! @brief 追加一条日志并由 QML 面板展示
    //! @param level 级别名（DEBUG/INFO/WARN/ERROR/FATAL 等），决定整行主色
    //! @param message 原始文本，内部统一转义，避免 HTML 注入
    //! @param source 来源名（如 QML）；非空时行内 [source] 标签以来源色标出
    Q_INVOKABLE void appendMessage(const QString& level, const QString& message, const QString& source = QString());

signals:
    void messagesChanged();
    void newMessage(const QString& level, const QString& message);

private:
    QStringList messages_;
    static constexpr int MaxMessages = 1000;
    static std::unique_ptr<QLogManager> self_;
};
