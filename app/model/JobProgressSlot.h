/**
 * @file JobProgressSlot.h
 * @brief 进度槽：worker 只写、GUI 定频轮询的进度合流点（替代每报一次跨线程投递）
 *
 * 轮询模型：worker 侧 report 每次调用仅写槽（短锁拷贝，近乎零成本），GUI 侧 QTimer
 * 定频 poll 取最新快照落地——N 次上报折叠为 ≤时长/轮询周期 次 UI 更新，事件队列不再
 * 被进度投递灌满；label→QString 转换也随之移到轮询侧（每轮询 1 次，替代每报 1 次）。
 *
 * 契约：
 * - last-wins 合并：中间帧自然丢弃，poll 只见最新值；
 * - 成对一致：value 与 label 在同一把锁下拷出，不会读到"新值配旧标签"；
 * - 终态门闩：close() 后 poll 恒空、write 被丢弃——"终态之后无进度事件"由本类保证
 *   （GUI 侧先 close 再呈现终态，随后到达的迟到写入一律作废）；
 * - reset()：新任务开始时清空待投递残留并解除门闩。
 */
#ifndef JOB_PROGRESS_SLOT_H
#define JOB_PROGRESS_SLOT_H

#include <mutex>
#include <string>
#include <utility>

class JobProgressSlot {
public:
    /**
     * @brief worker 线程写入最新进度（close 后的迟到写入被丢弃）
     */
    void write(double value, std::string label)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closed_)
            return;
        value_ = value;
        label_ = std::move(label);
        dirty_ = true;
    }

    /**
     * @brief GUI 线程轮询：有未消费新值返回 true 并拷出快照；无新值或已 close 返回 false
     */
    bool poll(double& value, std::string& label)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!dirty_ || closed_)
            return false;
        dirty_ = false;
        value = value_;
        label = label_;
        return true;
    }

    /**
     * @brief 任务终态门闩（GUI 线程，先于终态呈现调用）：此后 poll 不发、write 丢弃
     */
    void close()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        dirty_ = false;
    }

    /**
     * @brief 新任务开始（GUI 线程）：清空上一任务残留并解除门闩
     */
    void reset()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = false;
        dirty_ = false;
        value_ = 0.0;
        label_.clear();
    }

private:
    std::mutex mutex_;
    double value_ { 0.0 }; //> 最新进度值（0~1，钳位在轮询落地侧做）
    std::string label_; //> 最新进度文本（与 value 同锁成对）
    bool dirty_ { false }; //> 有未消费新值（写置位、poll 清零）
    bool closed_ { false }; //> 终态门闩（close 置位、reset 解除）
};

#endif
