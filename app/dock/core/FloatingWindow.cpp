/**
 * @file FloatingWindow.cpp
 * @brief 浮动窗口的实现
 */

#include "FloatingWindow.h"

#include "DockRegistry.h"
#include "DropArea.h"
#include "Group.h"
#include "engine/Item.h"

namespace dock {

FloatingWindow::FloatingWindow(QObject* parent)
    : Controller(Type::FloatingWindow, parent)
    , drop_area_(new DropArea(this))
{
    DockRegistry::self().registerFloatingWindow(this);
}

FloatingWindow::~FloatingWindow()
{
    DockRegistry::self().unregisterFloatingWindow(this);
}

void FloatingWindow::takeGroup(Group* group, Item* item)
{
    group_ = group;
    if (!group_) {
        drop_area_->setRootItem(nullptr);
        return;
    }

    drop_area_->setRootItem(item);
    if (item) {
        item->setId(group_->title());
        group_->setLayoutItem(item);
    }
    group_->setFloating(true);
    Q_EMIT titleChanged(title());
}

Item* FloatingWindow::releaseGroup()
{
    if (!group_)
        return nullptr;

    group_->setFloating(false);
    group_ = nullptr;
    return drop_area_->takeRootItem();
}

bool FloatingWindow::isEmpty() const
{
    return !group_ || group_->dockWidgets().isEmpty();
}

void FloatingWindow::setGeometry(const QRect& geometry)
{
    if (geometry_ == geometry)
        return;

    geometry_ = geometry;
    drop_area_->setGlobalOrigin(geometry.topLeft());
    drop_area_->setGeometry(QRect(QPoint(0, 0), geometry.size()));
    Q_EMIT geometryChanged(geometry_);
}

QString FloatingWindow::title() const
{
    return group_ ? group_->title() : QString();
}

void FloatingWindow::close()
{
    Q_EMIT closed();
}

}
