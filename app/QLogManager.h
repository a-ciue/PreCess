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

    Q_INVOKABLE void appendMessage(const QString& level, const QString& message);

signals:
    void messagesChanged();
    void newMessage(const QString& level, const QString& message);

private:
    QStringList messages_;
    static constexpr int MaxMessages = 1000;
    static std::unique_ptr<QLogManager> self_;
};
