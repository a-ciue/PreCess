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
    //! @param message 已转义的 HTML 片段（调用方负责转义原始文本）
    Q_INVOKABLE void appendMessage(const QString& level, const QString& message);

signals:
    void messagesChanged();
    void newMessage(const QString& level, const QString& message);

private:
    QStringList messages_;
    static constexpr int MaxMessages = 1000;
    static std::unique_ptr<QLogManager> self_;
};
