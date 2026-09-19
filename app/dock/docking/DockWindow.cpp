/**
 * @file DockWindow.cpp
 * @brief 浮动窗口的实现
 */

#include "DockWindow.h"

#include "DockCatalog.h"
#include "DockRegion.h"
#include "PanelGroup.h"
#include "tree/LayoutNode.h"

namespace dock {

DockWindow::DockWindow(QObject* parent)
    : DockObject(Kind::Window, parent)
    , region_(new DockRegion(this))
{
    DockCatalog::self().registerWindow(this);
}

DockWindow::~DockWindow()
{
    DockCatalog::self().unregisterWindow(this);
}

void DockWindow::takeGroup(PanelGroup* group, LayoutNode* item)
{
    group_ = group;
    if (!group_) {
        region_->setRootNode(nullptr);
        return;
    }

    region_->setRootNode(item);
    if (item) {
        item->setId(group_->title());
        group_->setNode(item);
    }
    group_->setDetached(true);
    Q_EMIT titleChanged(title());
}

LayoutNode* DockWindow::releaseGroup()
{
    if (!group_)
        return nullptr;

    group_->setDetached(false);
    group_ = nullptr;
    return region_->takeRootNode();
}

bool DockWindow::isEmpty() const
{
    return !group_ || group_->panels().isEmpty();
}

void DockWindow::setGeometry(const QRect& geometry)
{
    if (geometry_ == geometry)
        return;

    geometry_ = geometry;
    region_->setGlobalOrigin(geometry.topLeft());
    region_->setGeometry(QRect(QPoint(0, 0), geometry.size()));
    Q_EMIT geometryChanged(geometry_);
}

QString DockWindow::title() const
{
    return group_ ? group_->title() : QString();
}

void DockWindow::close()
{
    Q_EMIT closed();
}

}
