/**
 * @file GeometryTopologyDiagnosticTaskQueue.h
 * @brief 在后台线程串行执行几何拓扑诊断，避免长时间计算阻塞渲染线程。
 */
#pragma once

#include "GeometryTopologyEditor.h"

#include <TopoDS_Shape.hxx>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

/**
 * @brief 一次诊断请求的不可变输入快照。
 *
 * `shape` 持有 TopoDS_Shape 的值拷贝，靠 TShape 引用计数保证后台计算期间底层几何
 * 不会被释放；阈值与 options 也一并复制，避免后台读取到被渲染线程改写中的状态。
 */
struct GeometryTopologyDiagnosticRequest {
    //! 请求方标识（组件诊断对象的地址），结果回传时用于区分归属。
    const void* owner {};
    //! 请求方给出的代号；形状或阈值变化后递增，旧结果据此丢弃。
    std::uint64_t generation {};
    //! 提交时刻，用于区分"排队等前一个任务"与"本任务真的算得久"。
    std::chrono::steady_clock::time_point submitted_at {};
    std::shared_ptr<const TopoDS_Shape> shape;
    //! 协作式取消标记：请求方置位后，任务在候选对之间退出，不中断单次 OCC 运算。
    std::shared_ptr<std::atomic<bool>> cancel;
    double small_edge_length_threshold {};
    double small_face_area_threshold {};
    GeometryTopologyDiagnosticOptions options {};
};

/** @brief 后台线程产出的诊断结果。 */
struct GeometryTopologyDiagnosticOutcome {
    const void* owner {};
    std::uint64_t generation {};
    GeometryTopologyDiagnosticOptions options {};
    GeometryTopologyDiagnosticResult result;
    //! 计算抛异常时为 true，调用方应保持"未计算"状态而不是显示空结果。
    bool failed { false };
    //! 被协作式取消时为 true；不计入失败，调用方直接丢弃即可。
    bool cancelled { false };
};

/**
 * @brief 串行执行几何拓扑诊断请求的后台队列。
 *
 * 任意时刻最多只有一个任务在运行：OCC 的 BOP 布尔栈不能被多个线程并发调用
 * （见交接文档第 14.2 节）。同一份形状、同一组参数的未启动重复请求会被合并；
 * 已经在运行的任务不做强制中断，其结果在回传时按 generation 丢弃。
 */
class GeometryTopologyDiagnosticTaskQueue {
public:
    static GeometryTopologyDiagnosticTaskQueue& shared();

    //! 提交一次诊断，返回本次请求的 generation。相同形状与参数的未启动请求会被替换。
    std::uint64_t submit(GeometryTopologyDiagnosticRequest request);

    //! 取出指定请求方已完成的结果；只能由渲染线程调用。
    std::vector<GeometryTopologyDiagnosticOutcome> takeOutcomes(const void* owner);

    //! 丢弃指定请求方尚未启动的请求与其已完成但未被取走的结果。
    void discardOwner(const void* owner);

    //! 是否还有任务在运行或排队，供界面显示"计算中"。
    bool isBusy();

    //! 是否还有任何未取走的请求或结果。
    bool hasWork();

    //! 是否有已完成、尚未取走的结果。渲染线程只在此时才需要跑一次搬运，
    //! 长任务"忙碌但无结果"期间不要反复投递空操作。
    bool hasReadyResults();

    //! 停止工作线程并等待其退出；析构时自动调用。
    void shutdown();

    GeometryTopologyDiagnosticTaskQueue(const GeometryTopologyDiagnosticTaskQueue&) = delete;
    GeometryTopologyDiagnosticTaskQueue& operator=(const GeometryTopologyDiagnosticTaskQueue&) = delete;

private:
    GeometryTopologyDiagnosticTaskQueue() = default;
    ~GeometryTopologyDiagnosticTaskQueue();

    /** @brief 已启动、可分段推进的一个任务。 */
    struct ActiveTask {
        GeometryTopologyDiagnosticRequest request;
        //! 分段推进会话；尚未建立时为 nullptr（建会话放在工作线程里做）。
        std::unique_ptr<GeometryTopologyDiagnosticSession> session;
        //! 首次拿到工作线程的时刻，用于区分"排队等前面的任务"与"自己算得久"。
        std::chrono::steady_clock::time_point started_at {};
        //! 已累计的计算时间（各时间片之和）。
        double spent_ms { 0.0 };
        //! 因有新请求在等而让出工作线程的次数（不含普通的时间片切换）。
        int preemptions { 0 };
    };

    //! 工作线程主体：取任务、推进一个时间片、按需让位或收尾。
    void run();
    //! 产出并投递结果；失败与取消分别由 failed / cancelled 标记。
    void publish(ActiveTask& task, bool cancelled, bool failed);

    //! 已析构请求方的地址：其迟到结果不再保留。同一地址提交新请求时解除。
    //! 只有"曾见证过该地址析构"才需要这个集合；旧结果本身由全局 generation 拒绝。
    std::set<const void*> discarded_owners_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::deque<GeometryTopologyDiagnosticRequest> pending_;
    //! 已推进过一段、被让位后等待继续的任务；取任务时 pending_ 优先，
    //! 因此有新请求在等就自然让位，没有就立刻继续推进同一任务。
    std::deque<std::unique_ptr<ActiveTask>> suspended_;
    std::vector<GeometryTopologyDiagnosticOutcome> ready_;
    std::thread worker_;
    bool stopping_ { false };
    bool busy_ { false };
    std::uint64_t next_generation_ { 1 };
    std::uint64_t executed_generation_ { 0 };
};
