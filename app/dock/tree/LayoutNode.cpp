/**
 * @file LayoutNode.cpp
 * @brief 布局树节点的实现
 */

#include "LayoutNode.h"

#include "BoxNode.h"
#include "LayoutClient.h"

namespace dock {

LayoutNode::LayoutNode(LayoutClient* client)
    : client_(client)
{
}

LayoutNode::~LayoutNode() = default;

void LayoutNode::setGeometry(const QRect& geometry)
{
    sizing_.geometry = geometry;
    if (client_)
        client_->applyGeometry(geometry);
}

void LayoutNode::setVisible(bool visible)
{
    if (visible_ == visible)
        return;

    setVisibleSilently(visible);
    if (parent_)
        parent_->handleChildVisibility(this, visible);
}

void LayoutNode::setVisibleSilently(bool visible)
{
    if (visible_ == visible)
        return;

    visible_ = visible;
    if (client_)
        client_->applyVisibility(visible);
}

QSize LayoutNode::minExtent() const
{
    return client_ ? client_->minExtent() : QSize(0, 0);
}

QSize LayoutNode::maxExtent() const
{
    return client_ ? client_->maxExtent() : QSize(kMaxSizeLimit, kMaxSizeLimit);
}

}
