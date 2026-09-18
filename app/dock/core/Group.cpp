/**
 * @file Group.cpp
 * @brief 选项卡分组的实现
 */

#include "Group.h"

#include "DockWidget.h"
#include "View.h"
#include "engine/Item.h"

#include <algorithm>
#include <utility>

namespace dock {

Group::Group(QObject* parent)
    : Controller(Type::Group, parent)
{
}

Group::~Group()
{
    for (DockWidget* dock_widget : std::as_const(dock_widgets_)) {
        if (dock_widget->group() == this)
            dock_widget->setGroup(nullptr);
    }
}

void Group::addDockWidget(DockWidget* dock_widget)
{
    if (!dock_widget || dock_widgets_.contains(dock_widget))
        return;

    dock_widgets_.append(dock_widget);
    dock_widget->setGroup(this);

    connect(dock_widget, &DockWidget::isOpenChanged, this, [this, dock_widget] {
        updateCurrentDockWidget();
        Q_EMIT dockWidgetsChanged();
        refreshVisibility();
        if (dock_widget == current_)
            Q_EMIT titleChanged(title());
    });
    connect(dock_widget, &DockWidget::titleChanged, this, [this, dock_widget](const QString&) {
        if (dock_widget == current_)
            Q_EMIT titleChanged(title());
    });

    if (!current_ || !current_->isOpen())
        current_ = dock_widget;

    Q_EMIT dockWidgetsChanged();
    Q_EMIT currentDockWidgetChanged(current_);
    refreshVisibility();
}

void Group::removeDockWidget(DockWidget* dock_widget)
{
    const int index = dock_widgets_.indexOf(dock_widget);
    if (index < 0)
        return;

    dock_widgets_.removeAt(index);
    if (dock_widget->group() == this)
        dock_widget->setGroup(nullptr);
    disconnect(dock_widget, nullptr, this, nullptr);

    if (current_ == dock_widget)
        current_ = dock_widgets_.isEmpty() ? nullptr : dock_widgets_.first();

    Q_EMIT dockWidgetsChanged();
    Q_EMIT currentDockWidgetChanged(current_);
    refreshVisibility();
}

QList<DockWidget*> Group::openDockWidgets() const
{
    QList<DockWidget*> result;
    for (DockWidget* dock_widget : dock_widgets_) {
        if (dock_widget->isOpen())
            result.append(dock_widget);
    }
    return result;
}

void Group::setCurrentDockWidget(DockWidget* dock_widget)
{
    if (!dock_widget || !dock_widget->isOpen() || !dock_widgets_.contains(dock_widget))
        return;
    if (current_ == dock_widget)
        return;

    current_ = dock_widget;
    Q_EMIT currentDockWidgetChanged(current_);
    Q_EMIT titleChanged(title());
}

int Group::currentIndex() const
{
    return openDockWidgets().indexOf(current_);
}

void Group::setCurrentIndex(int index)
{
    const QList<DockWidget*> open = openDockWidgets();
    if (index < 0 || index >= open.size())
        return;
    setCurrentDockWidget(open.at(index));
}

QString Group::title() const
{
    return current_ ? current_->title() : QString();
}

void Group::refreshVisibility()
{
    const bool has_open = !openDockWidgets().isEmpty();
    if (layout_item_) {
        if (layout_item_->isVisible() != has_open)
            layout_item_->setVisible(has_open);
    } else if (view()) {
        view()->setViewVisible(has_open);
    }
}

void Group::setFloating(bool floating)
{
    for (DockWidget* dock_widget : std::as_const(dock_widgets_))
        dock_widget->markFloating(floating);
}

void Group::setGuestGeometry(const QRect& geometry)
{
    if (view())
        view()->setViewGeometry(geometry);
}

QSize Group::minSize() const
{
    QSize result(0, 0);
    const QList<DockWidget*> open = openDockWidgets();
    for (DockWidget* dock_widget : open) {
        const View* guest = dock_widget->guestView();
        if (!guest)
            continue;
        const QSize guest_min = guest->viewMinSize();
        result = QSize(std::max(result.width(), guest_min.width()),
            std::max(result.height(), guest_min.height()));
    }
    return result;
}

QSize Group::maxSizeHint() const
{
    QSize result(kMaxSizeLimit, kMaxSizeLimit);
    const QList<DockWidget*> open = openDockWidgets();
    for (DockWidget* dock_widget : open) {
        const View* guest = dock_widget->guestView();
        if (!guest)
            continue;
        const QSize guest_max = guest->viewMaxSizeHint();
        result = QSize(std::min(result.width(), guest_max.width()),
            std::min(result.height(), guest_max.height()));
    }
    return result;
}

void Group::setGuestVisible(bool visible)
{
    if (view())
        view()->setViewVisible(visible);
}

void Group::updateCurrentDockWidget()
{
    if (current_ && current_->isOpen() && dock_widgets_.contains(current_))
        return;

    current_ = nullptr;
    for (DockWidget* dock_widget : std::as_const(dock_widgets_)) {
        if (dock_widget->isOpen()) {
            current_ = dock_widget;
            break;
        }
    }
    Q_EMIT currentDockWidgetChanged(current_);
}

}
