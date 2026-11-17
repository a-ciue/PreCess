/**
 * @file GlobalMouseRelay.cpp
 * @brief 全局输入事件过滤器的实现
 */

#include "GlobalMouseRelay.h"

#include "docking/DockCatalog.h"
#include "docking/DockPanel.h"
#include "docking/DragSession.h"
#include "docking/PanelGroup.h"

#include <QCoreApplication>
#include <QCursor>
#include <QKeyEvent>
#include <QMouseEvent>

namespace dock::ui {

GlobalMouseRelay::GlobalMouseRelay(QObject* parent)
    : QObject(parent)
{
}

GlobalMouseRelay* GlobalMouseRelay::instance()
{
    static GlobalMouseRelay* relay = new GlobalMouseRelay(QCoreApplication::instance());
    return relay;
}

bool GlobalMouseRelay::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched);

    dock::DragSession& drag = dock::DragSession::self();

    if (drag.phase() == dock::DragSession::Phase::Idle) {
        // 空闲时广播左键按下，应用层据此清理与点击区域相关的选中状态。
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouse_event = static_cast<QMouseEvent*>(event);
            if (mouse_event->button() == Qt::LeftButton)
                emit leftPressed(mouse_event->globalPosition());
        }

        // 空闲时仅关注面板切换快捷键
        if (event->type() == QEvent::KeyPress) {
            auto* key_event = static_cast<QKeyEvent*>(event);
            if (key_event->modifiers().testFlag(Qt::ControlModifier)
                && (key_event->key() == Qt::Key_Tab || key_event->key() == Qt::Key_Backtab)) {
                const bool forward = key_event->key() == Qt::Key_Tab
                    && !key_event->modifiers().testFlag(Qt::ShiftModifier);
                return cyclePanels(forward);
            }
        }
        return false;
    }

    switch (event->type()) {
    case QEvent::MouseMove: {
        auto* mouse_event = static_cast<QMouseEvent*>(event);
        drag.updateAt(mouse_event->globalPosition().toPoint());
        return true;
    }
    case QEvent::MouseButtonRelease: {
        auto* mouse_event = static_cast<QMouseEvent*>(event);
        if (mouse_event->button() != Qt::LeftButton)
            return false;
        drag.endAt(mouse_event->globalPosition().toPoint());
        return true;
    }
    case QEvent::KeyPress: {
        auto* key_event = static_cast<QKeyEvent*>(event);
        if (key_event->key() == Qt::Key_Escape) {
            drag.cancel();
            return true;
        }
        return false;
    }
    default:
        return false;
    }
}

bool GlobalMouseRelay::cyclePanels(bool forward)
{
    // 切换目标由鼠标悬停的分组决定（严格悬停驱动）
    return dock::DockCatalog::self().cyclePanelAt(QCursor::pos(), forward);
}

}
