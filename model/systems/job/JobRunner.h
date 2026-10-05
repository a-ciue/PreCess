/**
 * @file JobRunner.h
 * @brief 单槽执行器：worker 只计算，所属线程一次完成提交、清理和释放。
 */
#pragma once
#include "Job.h"
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

class ModelLayer;
class UndoStack;
namespace systems::job {
/** @brief GUI 准备结果；空 task 表示拒绝，inline_compute 用于不可影子化的同步算法。 */
struct JobWork {
    JobTaskFn task;
    JobCommitFn commit;
    bool inline_compute { false };
};
using JobPrepareFn = std::function<JobWork()>;
/** @brief 发布时固定的归属与展示策略；owner 不授予写权。 */
struct JobOptions {
    std::string owner;
    bool masked { false };
    bool prepare_command { false }; //!< 准备输入前回滚旧预览
};
/**
 * @brief 所属线程管理一个在飞任务；worker 不等待 GUI。
 * 发布、准备、查询、清理、终态与 stop 均在构造线程。
 * 异步运行须向该线程投递；计算结束后保持槽与模型占用直到 GUI 收尾。
 */
class JobRunner {
public:
    /** @brief 构造即绑定模型宿主与所属线程分发器；同步宿主无需执行器。 */
    JobRunner(ModelLayer& model, UndoStack* stack, std::function<void(std::function<void()>)> dispatcher);
    ~JobRunner();
    JobRunner(const JobRunner&) = delete;
    JobRunner& operator=(const JobRunner&) = delete;
    //! @brief 外借执行器注入系统时验证宿主一致，不修改绑定。
    void assertHost(const ModelLayer& model, const UndoStack* stack) const;
    //! @brief 先占用，再在所属线程准备；准备异常自动释放。
    std::shared_ptr<Job> run(std::string name, JobPrepareFn prepare, JobOptions options = { });
    bool deferUntilFinished(std::function<void()> cleanup);
    //! @brief 请求取消并 join；所属线程调用同一个收尾入口，不泵事件。
    void stop();
    std::shared_ptr<Job> currentJob() const;
    /** @brief 宿主统一任务展示；只在所属线程空闲时装配，回调对象须活到 stop 返回。 */
    void setOnStarted(JobFinishedFn callback);
    void setOnProgress(JobProgressFn callback);
    void setOnFinished(JobFinishedFn callback);

private:
    struct ModelOperation;
    void assertOwnerThread() const;
    void assertObserverChangeAllowed() const;
    static void notifyLifecycle(JobFinishedFn callback, Job& job);
    void workerLoop();
    void compute(const std::shared_ptr<Job>& job);
    void complete(std::shared_ptr<Job> job, bool inline_compute = false);
    const std::thread::id owner_thread_ { std::this_thread::get_id() };
    std::shared_ptr<int> lifetime_ { std::make_shared<int>(0) };
    std::function<void(std::function<void()>)> dispatcher_;
    ModelLayer& model_;
    UndoStack* const stack_;
    std::unique_ptr<ModelOperation> operation_;
    std::shared_ptr<Job> slot_; //!< 仅所属线程访问；计算完成不让槽
    bool completing_ { false };
    std::mutex sync_;
    std::condition_variable ready_;
    std::shared_ptr<Job> work_; //!< 单次计算交付，受 sync_ 保护
    bool stopping_ { false }; //!< 所属线程写，worker 在 sync_ 内读
    std::thread worker_;
    JobFinishedFn on_started_;
    JobProgressFn on_progress_;
    JobFinishedFn on_finished_;
};
}
