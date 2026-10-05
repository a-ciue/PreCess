/**
 * @file Job.h
 * @brief 任务单元：状态、错误、取消令牌与执行闭包（线程模型的任务载体）
 */
#pragma once
#include "JobProgress.h"

#include <exception>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <vector>

namespace systems::job {

/**
 * @brief 任务状态；由执行器固定的提交和收尾路径推进
 *
 * Running → Committing → Done / Failed；Running 也可直接 Done / Failed / Cancelled，终态不可再迁。
 * 无 Queued 态：调度为单工作进程 + 无队列——提交即占坑（Running）或被拒绝。
 */
enum class JobState {
    Running, //!< 执行中（占坑即此态：任务体执行或等待工作进程拾取）
    Committing, //!< 结果提交中（在提交分发线程，通常为 GUI 线程）
    Done, //!< 成功完成
    Failed, //!< 任务体或提交失败
    Cancelled, //!< 心跳点取消（含占坑后、执行前取消）
};

//! 终态判定
inline bool isTerminal(JobState state) noexcept
{
    return state == JobState::Done || state == JobState::Failed || state == JobState::Cancelled;
}

/**
 * @brief 心跳点取消异常：由包裹进度回调抛出
 * @note 任务无需捕获，沿调用链正常传播至 JobRunner；与项目异常策略一致。
 */
class JobCancelledException : public std::exception {
public:
    const char* what() const noexcept override { return "job cancelled"; }
};

class Job; // 前置声明：下方 JobProgressFn / JobFinishedFn 引用本类

//! 任务体只计算；业务成果由类型化执行闭包持有，失败抛异常。
using JobTaskFn = std::function<void(ProgressFn)>;
//! GUI 提交段；仅计算成功且未取消时执行，失败抛异常。
using JobCommitFn = std::function<void()>;
//! 进度回调（在执行任务体的线程触发）
using JobProgressFn = std::function<void(Job&, double, const std::string&)>;
//! 终态回调（在所属线程触发一次；单槽在回调前已让出）
using JobFinishedFn = std::function<void(Job&)>;

/**
 * @brief 任务单元：持有执行体、提交体、取消令牌与状态
 *
 * 状态迁移仅由 Runner 的固定收尾路径推进，读写由互斥保护；Runner 与调用方约定：
 * 只有 Runner 驱动状态迁移；调用方只读状态与错误。
 */
class Job {
public:
    explicit Job(std::string name, std::string owner = { }, bool masked = false)
        : name_(std::move(name))
        , owner_(std::move(owner))
        , masked_(masked)
    {
    }

    const std::string& name() const noexcept { return name_; }
    const std::string& owner() const noexcept { return owner_; }
    bool masked() const noexcept { return masked_; }
    JobState state() const
    {
        std::lock_guard lock(mutex_);
        return state_;
    }
    std::string error() const
    {
        std::lock_guard lock(mutex_);
        return error_;
    }
    //! @brief 只读取消请求；请求不代表任务已退出或占用已释放。
    bool isCancellationRequested() const noexcept { return stop_source_.stop_requested(); }

    //! 请求取消（线程安全，任意时刻可调；任务体于下次心跳感知）
    void cancel() noexcept { stop_source_.request_stop(); }

private:
    friend class JobRunner;
    //! @brief 仅供执行器的提交与终态两个固定位置写入状态。
    void setState(JobState state)
    {
        std::lock_guard lock(mutex_);
        state_ = state;
    }

    void setError(std::string error)
    {
        std::lock_guard lock(mutex_);
        error_ = std::move(error);
    }

    std::vector<std::function<void()>> finalizers; //!< GUI 清理，仅由所属线程登记和消费

    // 任务执行要素由提交方在占坑前填入，执行期只读
    JobTaskFn task;
    JobCommitFn commit;

    const std::string name_;
    const std::string owner_;
    const bool masked_;
    mutable std::mutex mutex_;
    JobState state_ { JobState::Running };
    std::string error_;
    std::stop_source stop_source_;
};
}
