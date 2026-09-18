/**
 * @file MouseGrabber.cpp
 * @brief 全局鼠标事件过滤器的实现
 */

#include "MouseGrabber.h"

#include "core/DragController.h"

#include <QMouseEvent>

namespace dock::qtquick {

MouseGrabber::MouseGrabber(QObject* parent)
    : QObject(parent)
{
}

bool MouseGrabber::eventFilter(QObject* watched, QEvent* event)
{
    Q_UNUSED(watched);

    dock::DragController& drag = dock::DragController::self();
    if (drag.state() == dock::DragController::State::Idle)
        return false;

    switch (event->type()) {
    case QEvent::MouseMove: {
        auto* mouse_event = static_cast<QMouseEvent*>(event);
        drag.onMove(mouse_event->globalPosition().toPoint());
        return true;
    }
    case QEvent::MouseButtonRelease: {
        auto* mouse_event = static_cast<QMouseEvent*>(event);
        if (mouse_event->button() != Qt::LeftButton)
            return false;
        drag.onRelease(mouse_event->globalPosition().toPoint());
        return true;
    }
    default:
        return false;
    }
}

}
