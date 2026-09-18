/**
 * @file Item.cpp
 * @brief 布局树节点的实现
 */

#include "Item.h"

#include "ItemBoxContainer.h"
#include "LayoutingGuest.h"

namespace dock {

Item::Item(LayoutingGuest* guest)
    : guest_(guest)
{
}

Item::~Item() = default;

void Item::setGeometry(const QRect& geometry)
{
    sizing_.geometry = geometry;
    if (guest_)
        guest_->setGuestGeometry(geometry);
}

void Item::setVisible(bool visible)
{
    if (visible_ == visible)
        return;

    setVisibleSilently(visible);
    if (parent_)
        parent_->onChildVisibilityChanged(this, visible);
}

void Item::setVisibleSilently(bool visible)
{
    if (visible_ == visible)
        return;

    visible_ = visible;
    if (guest_)
        guest_->setGuestVisible(visible);
}

QSize Item::minSize() const
{
    return guest_ ? guest_->minSize() : QSize(0, 0);
}

QSize Item::maxSizeHint() const
{
    return guest_ ? guest_->maxSizeHint() : QSize(kMaxSizeLimit, kMaxSizeLimit);
}

}
