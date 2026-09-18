/**
 * @file GuestView.cpp
 * @brief guest 视图适配器的实现
 */

#include "GuestView.h"

#include "engine/SizingInfo.h"

#include <QCursor>
#include <QQuickItem>
#include <QtMath>

namespace dock::qtquick {

GuestView::GuestView(Controller* controller, QQuickItem* guest_item)
    : controller_(controller)
    , guest_item_(guest_item)
{
}

void GuestView::setViewGeometry(const QRect& geometry)
{
    if (guest_item_)
        guest_item_->setSize(geometry.size());
}

QRect GuestView::viewGeometry() const
{
    if (!guest_item_)
        return {};
    return QRect(guest_item_->position().toPoint(), guest_item_->size().toSize());
}

void GuestView::setViewVisible(bool visible)
{
    if (guest_item_)
        guest_item_->setVisible(visible);
}

bool GuestView::isViewVisible() const
{
    return guest_item_ && guest_item_->isVisible();
}

QSize GuestView::viewMinSize() const
{
    if (!guest_item_)
        return QSize(0, 0);
    return QSize(qCeil(guest_item_->implicitWidth()), qCeil(guest_item_->implicitHeight()));
}

QSize GuestView::viewMaxSizeHint() const
{
    return QSize(kMaxSizeLimit, kMaxSizeLimit);
}

void GuestView::raiseView()
{
    if (guest_item_)
        guest_item_->setZ(guest_item_->z() + 1);
}

void GuestView::setViewCursor(Qt::CursorShape shape)
{
    if (guest_item_)
        guest_item_->setCursor(QCursor(shape));
}

Qt::CursorShape GuestView::viewCursor() const
{
    return guest_item_ ? guest_item_->cursor().shape() : Qt::ArrowCursor;
}

QPoint GuestView::viewGlobalPosition() const
{
    return guest_item_ ? guest_item_->mapToGlobal(QPointF(0, 0)).toPoint() : QPoint(0, 0);
}

View* GuestView::createFloatingWindowView(Controller* controller)
{
    Q_UNUSED(controller);
    return nullptr;
}

}
