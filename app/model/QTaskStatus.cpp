/** @file QTaskStatus.cpp
 * @brief 计算与提交共用一个进度槽，GUI 统一呈现执行反馈。
 */
#include "QTaskStatus.h"
#include "JobRunner.h"
#include "Session.h"
#include <QTimer>
#include <stdexcept>

QTaskStatus::QTaskStatus(QObject* parent)
    : QObject(parent)
{
    progress_timer_ = new QTimer(this);
    progress_timer_->setInterval(33);
    connect(progress_timer_, &QTimer::timeout, this, [this] {
        if (consumeProgress())
            emit progressChanged();
    });
}
void QTaskStatus::bindSession(session::Session& session)
{
    if (runner_ || !session.jobRunner())
        throw std::runtime_error("QTaskStatus: requires an unbound asynchronous session");
    session.setTaskCallbacks([this](systems::job::Job& job) { startJob(job); },
        [this](systems::job::Job&, double value, const std::string& label) {
            progress_slot_.write(value, label);
        },
        [this](systems::job::Job& job) { finishJob(job); });
    runner_ = session.jobRunner();
}
void QTaskStatus::startJob(systems::job::Job& job)
{
    progress_slot_.reset();
    progress_timer_->start();
    progress_ = 0.0;
    message_.clear();
    cancel_requested_ = false;
    running_ = true;
    masked_ = job.masked();
    emit runningChanged();
    emit progressChanged();
    emit busyChanged();
    emit taskStarted();
}
void QTaskStatus::finishJob(systems::job::Job& job)
{
    // worker 已退出、GUI 提交已结束；短任务也必须消费最后一句反馈。
    cancel_requested_ = false;
    consumeProgress();
    progress_slot_.close();
    progress_timer_->stop();
    running_ = false;
    const auto state = job.state();
    if (state == systems::job::JobState::Done) {
        progress_ = 1.0;
        if (message_.isEmpty())
            message_ = tr("已完成");
    } else if (state == systems::job::JobState::Failed)
        message_ = tr("执行失败：") + QString::fromStdString(job.error());
    else if (state == systems::job::JobState::Cancelled)
        message_ = tr("已取消");
    emit runningChanged();
    emit progressChanged();
    emit busyChanged();
    if (state == systems::job::JobState::Failed)
        emit taskFailed(QString::fromStdString(job.error()));
    else if (state == systems::job::JobState::Cancelled)
        emit taskCancelled();
    emit taskFinished();
}
void QTaskStatus::reportFailure(const QString& error)
{
    showMessage(tr("执行失败：") + error);
    emit taskFailed(error);
}
void QTaskStatus::showMessage(const QString& text)
{
    if (running_ || text.isEmpty())
        return;
    message_ = text;
    emit progressChanged();
}
bool QTaskStatus::consumeProgress()
{
    double value;
    std::string label;
    if (!progress_slot_.poll(value, label))
        return false;
    progress_ = qBound(0.0, value, 1.0);
    if (!cancel_requested_ && !label.empty())
        message_ = QString::fromStdString(label);
    return true;
}
void QTaskStatus::cancel()
{
    if (!running_ || !runner_)
        return;
    auto job = runner_->currentJob();
    if (!job)
        return;
    cancel_requested_ = true;
    message_ = tr("正在取消…");
    emit progressChanged();
    job->cancel();
}
