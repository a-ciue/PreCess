/**
 * @file JobRunner.cpp
 * @brief 计算结果先发布，再投递一次所属线程收尾；stop 复用收尾。
 */
#include "JobRunner.h"
#include "ModelLayer.h"
#include "UndoStack.h"
#include <cassert>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>
namespace systems::job {
struct JobRunner::ModelOperation {
    std::unique_ptr<ModelLayer::WriteOperation> lease;
    std::optional<UndoStack::Capture> capture;
};
JobRunner::JobRunner(ModelLayer& model, UndoStack* stack, std::function<void(std::function<void()>)> dispatcher)
    : dispatcher_(std::move(dispatcher))
    , model_(model)
    , stack_(stack)
{
    model_.assertOwnerThread();
    if (!dispatcher_)
        throw std::runtime_error("JobRunner: requires an owner-thread dispatcher");
}
JobRunner::~JobRunner()
{
    stop();
    lifetime_.reset();
}
void JobRunner::assertOwnerThread() const
{
    if (std::this_thread::get_id() != owner_thread_)
        throw std::runtime_error("JobRunner: operation requires the owner thread");
}
void JobRunner::assertHost(const ModelLayer& model, const UndoStack* stack) const
{
    assertOwnerThread();
    if (&model_ != &model || stack_ != stack)
        throw std::runtime_error("JobRunner: system and runner must share the same model host");
}
std::shared_ptr<Job> JobRunner::run(std::string name, JobPrepareFn prepare,
    JobOptions options)
{
    assertOwnerThread();
    if (stopping_ || slot_ || (stack_ && (options.continue_operation ? stack_->operation_ && stack_->operation_->writes_started : stack_->hasPendingOperationWrites())))
        return nullptr;
    auto job = std::make_shared<Job>(std::move(name), std::move(options.owner), options.masked);
    auto lease = model_.beginWriteOperation(options.masked);
    if (!lease)
        return nullptr;
    operation_ = std::make_unique<ModelOperation>(ModelOperation { std::move(lease) });
    slot_ = job;
    completing_ = true;
    try {
        if (options.prepare_command && stack_) {
            ModelLayer::WritePrivilege privilege(model_, operation_->lease.get());
            stack_->prepareOperation();
        }
        JobWork work = prepare();
        if (!work.task) {
            operation_.reset();
            slot_.reset();
            completing_ = false;
            return nullptr;
        }
        job->task = std::move(work.task);
        job->commit = std::move(work.commit);
        if (work.inline_compute) {
            notifyLifecycle(on_started_, *job);
            {
                ModelLayer::WritePrivilege privilege(model_, operation_->lease.get());
                compute(job);
            }
            completing_ = false;
            complete(job, true);
        } else {
            if (!worker_.joinable())
                worker_ = std::thread(&JobRunner::workerLoop, this);
            if (options.continue_operation && stack_ && stack_->operation_)
                operation_->capture = stack_->detachOperation();
            notifyLifecycle(on_started_, *job);
            {
                std::lock_guard lock(sync_);
                work_ = job;
            }
            completing_ = false;
            ready_.notify_one();
        }
    } catch (...) {
        operation_.reset();
        slot_.reset();
        completing_ = false;
        throw;
    }
    return job;
}
std::shared_ptr<Job> JobRunner::currentJob() const
{
    assertOwnerThread();
    return slot_;
}
bool JobRunner::deferUntilFinished(std::function<void()> cleanup)
{
    assertOwnerThread();
    if (!slot_)
        return false;
    slot_->finalizers.push_back(std::move(cleanup));
    return true;
}
void JobRunner::stop()
{
    assertOwnerThread();
    if (completing_)
        throw std::runtime_error("JobRunner: stop cannot reenter task completion");
    if (slot_)
        slot_->cancel();
    {
        std::lock_guard lock(sync_);
        stopping_ = true;
    }
    ready_.notify_one();
    if (worker_.joinable())
        worker_.join();
    work_.reset(); // join 后无需再保留尚未拾取的输入。
    if (slot_)
        complete(slot_);
}
void JobRunner::workerLoop()
{
    for (;;) {
        std::shared_ptr<Job> job;
        {
            std::unique_lock lock(sync_);
            ready_.wait(lock, [this] { return stopping_ || work_; });
            if (stopping_)
                return;
            job = std::exchange(work_, { });
        }
        compute(job);
        try {
            dispatcher_([this, alive = std::weak_ptr<int>(lifetime_), job] {
                if (!alive.expired())
                    complete(job);
            });
        } catch (const std::exception& e) {
            // 结果仍在当前任务，由宿主 stop 消费；worker 不释放或清理模型。
            spdlog::error("JobRunner: completion dispatch failed: {}", e.what());
        } catch (...) {
            spdlog::error("JobRunner: completion dispatch failed with unknown exception");
        }
    }
}
void JobRunner::compute(const std::shared_ptr<Job>& job)
{
    std::string error;
    try {
        if (job->isCancellationRequested())
            throw JobCancelledException { };
        job->task([this, job](double value, const std::string& label) {
            if (job->isCancellationRequested())
                throw JobCancelledException { };
            if (on_progress_)
                on_progress_(*job, value, label);
        });
    } catch (const JobCancelledException&) {
        job->cancel();
    } catch (const std::exception& e) {
        error = e.what();
    } catch (...) {
        error = "unknown exception";
    }
    job->setError(std::move(error));
}
void JobRunner::complete(std::shared_ptr<Job> job, bool inline_compute)
{
    assertOwnerThread();
    if (slot_ != job || completing_)
        return;
    completing_ = true;
    std::string error = job->error();
    JobState terminal = JobState::Done;
    if (job->isCancellationRequested()) {
        error.clear();
        terminal = JobState::Cancelled;
    } else if (!error.empty())
        terminal = JobState::Failed;
    else if (job->commit) {
        try {
            if (!inline_compute && stack_ && stack_->inOperation())
                throw ModelOperationBusy("JobRunner: commit rejected while an operation boundary is open");
            assert(job->state() == JobState::Running);
            job->setState(JobState::Committing);
            ModelLayer::WritePrivilege privilege(model_, operation_->lease.get());
            if (operation_->capture) {
                stack_->resumeOperation(std::move(*operation_->capture));
                operation_->capture.reset();
            }
            job->commit();
        } catch (const std::exception& e) {
            error = e.what();
        } catch (...) {
            error = "unknown exception in commit";
        }
        if (!error.empty())
            terminal = JobState::Failed;
    }
    if (operation_->capture) {
        try {
            ModelLayer::WritePrivilege privilege(model_, operation_->lease.get());
            stack_->discardOperation(*operation_->capture);
        } catch (const std::exception& e) {
            error = e.what();
            terminal = JobState::Failed;
        }
        operation_->capture.reset();
    }
    // 计算与应用已退出；先在 GUI 销毁插件闭包，再执行可能注销 handler 的清理。
    job->task = { };
    job->commit = { };
    job->setError(std::move(error));
    // 清理中仍可登记清理；整个 drain 在同一所属线程段完成。
    while (!job->finalizers.empty()) {
        auto finalizers = std::exchange(job->finalizers, { });
        ModelLayer::WritePrivilege privilege(model_, operation_->lease.get());
        for (auto& cleanup : finalizers) {
            try {
                cleanup();
            } catch (const std::exception& e) {
                spdlog::error("JobRunner: cleanup failed: {}", e.what());
            } catch (...) {
                spdlog::error("JobRunner: cleanup failed with unknown exception");
            }
        }
    }
    assert(!isTerminal(job->state()));
    job->setState(terminal);
    operation_.reset();
    slot_.reset();
    completing_ = false;
    // 唯一宿主终态流程：收旧任务状态，再执行模型事件等业务收尾。
    if (terminal == JobState::Failed)
        spdlog::error("JobRunner: job '{}' failed: {}", job->name(), job->error());
    notifyLifecycle(on_finished_, *job);
}
void JobRunner::assertObserverChangeAllowed() const
{
    assertOwnerThread();
    if (slot_)
        throw std::runtime_error("JobRunner: observer cannot change while a job is active");
}
void JobRunner::setOnStarted(JobFinishedFn callback)
{
    assertObserverChangeAllowed();
    on_started_ = std::move(callback);
}
void JobRunner::setOnProgress(JobProgressFn callback)
{
    assertObserverChangeAllowed();
    on_progress_ = std::move(callback);
}
void JobRunner::setOnFinished(JobFinishedFn callback)
{
    assertObserverChangeAllowed();
    on_finished_ = std::move(callback);
}
void JobRunner::notifyLifecycle(JobFinishedFn callback, Job& job)
{
    if (!callback)
        return;
    try {
        callback(job);
    } catch (const std::exception& e) {
        spdlog::error("JobRunner: lifecycle callback failed: {}", e.what());
    } catch (...) {
        spdlog::error("JobRunner: lifecycle callback failed with unknown exception");
    }
}
}
