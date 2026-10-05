/** @file QTaskStatus.cpp
 * @brief worker 只写一个进度槽，GUI 统一呈现所有任务。
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
        double value;
        std::string label;
        if (!progress_slot_.poll(value, label))
            return;
        progress_ = qBound(0.0, value, 1.0);
        if (!cancel_requested_)
            progress_label_ = QString::fromStdString(label);
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
    progress_label_.clear();
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
    progress_slot_.close();
    progress_timer_->stop();
    running_ = false;
    cancel_requested_ = false;
    if (job.state() == systems::job::JobState::Done)
        progress_ = 1.0;
    emit runningChanged();
    emit progressChanged();
    emit busyChanged();
    if (job.state() == systems::job::JobState::Failed)
        reportFailure(QString::fromStdString(job.error()));
    else if (job.state() == systems::job::JobState::Cancelled)
        emit taskCancelled();
    emit taskFinished();
}
void QTaskStatus::reportFailure(const QString& error)
{
    emit taskFailed(error);
}
void QTaskStatus::cancel()
{
    if (!running_ || !runner_)
        return;
    auto job = runner_->currentJob();
    if (!job)
        return;
    cancel_requested_ = true;
    progress_label_ = tr("正在取消…");
    emit progressChanged();
    job->cancel();
}
