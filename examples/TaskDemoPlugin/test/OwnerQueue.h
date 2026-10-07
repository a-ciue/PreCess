/** @file OwnerQueue.h
 * @brief 测试用所属线程队列：worker 只投递，主线程明确消费一次收尾。
 * @note 复制自 PreCess model/systems/job/test/OwnerQueue.h（LGPLv3）。
 */
#pragma once
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <vector>

struct OwnerQueue {
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<std::function<void()>> pending;
    auto dispatcher()
    {
        return [this](std::function<void()> fn) {
            std::lock_guard lock(mutex);
            pending.push_back(std::move(fn));
            changed.notify_all();
        };
    }
    std::function<void()> take()
    {
        std::unique_lock lock(mutex);
        if (!changed.wait_for(lock, std::chrono::seconds(5), [this] { return !pending.empty(); }))
            throw std::runtime_error("completion did not arrive");
        auto fn = std::move(pending.front());
        pending.erase(pending.begin());
        return fn;
    }
};
