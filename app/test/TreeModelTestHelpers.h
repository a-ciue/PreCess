#pragma once

#include "TreeModel.h"

#include <QEventLoop>
#include <QTimer>

inline bool waitForDataSync(TreeModel& tree)
{
    QEventLoop loop;
    bool synced = false;
    QObject::connect(&tree, &TreeModel::dataSyncFinished, &loop, [&] {
        synced = true;
        loop.quit();
    });
    QTimer::singleShot(2000, &loop, &QEventLoop::quit);
    loop.exec();
    return synced;
}
