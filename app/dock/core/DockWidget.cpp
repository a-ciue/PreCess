/**
 * @file DockWidget.cpp
 * @brief 逻辑停靠面板的实现
 */

#include "DockWidget.h"

#include "DockRegistry.h"
#include "Group.h"

namespace dock {

DockWidget::DockWidget(const QString& unique_name, QObject* parent)
    : Controller(Type::DockWidget, parent)
    , unique_name_(unique_name)
    , title_(unique_name)
{
    DockRegistry::self().registerDockWidget(this);
}

DockWidget::~DockWidget()
{
    DockRegistry::self().unregisterDockWidget(this);
}

void DockWidget::setTitle(const QString& title)
{
    if (title_ == title)
        return;

    title_ = title;
    Q_EMIT titleChanged(title_);
}

void DockWidget::setGuestView(View* guest_view)
{
    guest_view_ = guest_view;
}

void DockWidget::open()
{
    markOpen(true);
}

void DockWidget::close()
{
    markOpen(false);
}

void DockWidget::setAsCurrentTab()
{
    if (group_)
        group_->setCurrentDockWidget(this);
}

void DockWidget::markOpen(bool open)
{
    const State target = open ? State::Docked : State::Hidden;
    if (state_ == target)
        return;

    if (state_ == State::Floating && open)
        return; // 浮动状态下的 open 由浮动窗口置顶处理

    state_ = target;
    if (group_)
        group_->refreshVisibility();

    Q_EMIT isOpenChanged(open);
    if (!open)
        Q_EMIT closed();
}

}
