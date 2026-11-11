#pragma once

#include "TreeModel.h"

#include <QEventLoop>
#include <QTimer>
#include <functional>

//! @brief 轮询等待目标状态出现；仅测试同步使用，不修改被测模型。
inline bool waitForCondition(const std::function<bool()>& predicate, int timeout_ms = 2000)
{
    if (predicate())
        return true;

    QEventLoop loop;
    bool satisfied = false;
    QTimer poll_timer;
    poll_timer.setInterval(1);
    QObject::connect(&poll_timer, &QTimer::timeout, &loop, [&] {
        if (predicate()) {
            satisfied = true;
            loop.quit();
        }
    });
    QTimer::singleShot(timeout_ms, &loop, &QEventLoop::quit);
    poll_timer.start();
    loop.exec();
    return satisfied;
}
