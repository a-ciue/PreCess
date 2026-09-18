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

void FloatingWindow::setGroup(Group* group)
{
    if (group_ == group)
        return;

    group_ = group;
    delete group_item_;
    group_item_ = nullptr;

    if (group_) {
        group_item_ = new Item(group_);
        group_item_->setId(group_->title());
        group_->setLayoutItem(group_item_);
        drop_area_->setRootItem(group_item_);
    } else {
        drop_area_->setRootItem(nullptr);
    }
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
