/**
 * @file Divider.cpp
 * @brief 分隔条的实现
 */

#include "Divider.h"

#include "LayoutNode.h"
#include "BoxNode.h"

#include <algorithm>

namespace dock {

Divider::Divider(BoxNode* container, LayoutNode* side1, LayoutNode* side2)
    : container_(container)
    , side1_(side1)
    , side2_(side2)
{
}

int Divider::position() const
{
    if (!container_ || !side1_)
        return 0;

    const Qt::Orientation axis = container_->orientation();
    return side1_->position(axis) + side1_->length(axis);
}

int Divider::minPosition() const
{
    if (!container_ || !side1_)
        return 0;

    const Qt::Orientation axis = container_->orientation();
    const QSize min_size = side1_->minExtent();
    return side1_->position(axis) + (axis == Qt::Horizontal ? min_size.width() : min_size.height());
}

int Divider::maxPosition() const
{
    if (!container_ || !side2_)
        return 0;

    const Qt::Orientation axis = container_->orientation();
    const QSize min_size = side2_->minExtent();
    const int side2_min = axis == Qt::Horizontal ? min_size.width() : min_size.height();
    return side2_->position(axis) + side2_->length(axis) - kDividerThickness - side2_min;
}

bool Divider::move(int newPosition)
{
    if (!container_ || !side1_ || !side2_)
        return false;

    const int min_position = minPosition();
    const int max_position = std::max(min_position, maxPosition());
    const int target = std::clamp(newPosition, min_position, max_position);
    if (target == position())
        return false;

    const Qt::Orientation axis = container_->orientation();
    const int side1_length = target - side1_->position(axis);
    const int side2_end = side2_->position(axis) + side2_->length(axis);
    const int side2_length = side2_end - target - kDividerThickness;

    container_->applyDividerMove(this, side1_length, side2_length);
    return true;
}

}
