/**
 * @file QTaskStatus.h
 * @brief 全局单槽任务的 Qt 展示：唯一进度合流、忙状态、取消入口与终态反馈。
 */
#pragma once
#include "JobProgressSlot.h"
#include <QObject>
#include <QString>
#include <QtQmlIntegration/qqmlintegration.h>

class QTimer;
namespace session {
class Session;
}
namespace systems::job {
class Job;
class JobRunner;
}

/** @brief GUI 展示对象；宿主必须先 stop Runner，再销毁本对象。 */
class QTaskStatus : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("QTaskStatus is provided by C++")
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(double progress READ getProgress NOTIFY progressChanged)
    Q_PROPERTY(QString progressLabel READ getProgressLabel NOTIFY progressChanged)
    Q_PROPERTY(bool writePending READ isWritePending NOTIFY busyChanged)
    Q_PROPERTY(bool frozenBusy READ isFrozenBusy NOTIFY busyChanged)
public:
    explicit QTaskStatus(QObject* parent = nullptr);
    //! @brief 空闲时一次装配唯一 Runner；模型操作与展示采用同一任务来源。
    void bindSession(session::Session& session);
    //! @brief 准备阶段异常尚未产生任务，宿主仍可显示失败。
    void reportFailure(const QString& error);
    Q_INVOKABLE void cancel();
    bool isRunning() const { return running_; }
    double getProgress() const { return progress_; }
    QString getProgressLabel() const { return progress_label_; }
    bool isWritePending() const { return running_; }
    bool isFrozenBusy() const { return running_ && masked_; }
signals:
    void runningChanged();
    void progressChanged();
    void busyChanged();
    void taskStarted();
    void taskFinished();
    void taskFailed(const QString& error);
    void taskCancelled();

private:
    void startJob(systems::job::Job& job);
    void finishJob(systems::job::Job& job);
    systems::job::JobRunner* runner_ { nullptr };
    JobProgressSlot progress_slot_;
    QTimer* progress_timer_ { nullptr };
    double progress_ { 0.0 };
    QString progress_label_;
    bool running_ { false };
    bool masked_ { false };
    bool cancel_requested_ { false };
};
