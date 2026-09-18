/**
 * @file DropArea.cpp
 * @brief 停靠区域的实现
 */

#include "DropArea.h"

#include "DockWidget.h"
#include "Group.h"
#include "View.h"
#include "engine/Item.h"
#include "engine/ItemBoxContainer.h"

namespace dock {

DropArea::DropArea(QObject* parent)
    : Controller(Type::None, parent)
{
}

DropArea::~DropArea()
{
    delete root_item_;
}

void DropArea::setRootItem(Item* item)
{
    if (root_item_ == item)
        return;

    delete root_item_;
    root_item_ = item;
    if (root_item_)
        root_item_->setGeometry(geometry_);
}

void DropArea::setGeometry(const QRect& geometry)
{
    geometry_ = geometry;
    if (root_item_)
        root_item_->setGeometry(geometry);
}

void DropArea::addDockWidget(DockWidget* dock_widget, Location location,
    DockWidget* relative_to, const QSize& preferred_size, InitialVisibilityOption option)
{
    if (!dock_widget)
        return;

    auto* group = new Group(this);
    group->addDockWidget(dock_widget);

    auto* item = new Item(group);
    item->setId(dock_widget->uniqueName());
    group->setLayoutItem(item);

    Item* relative_item = relative_to && relative_to->group() ? relative_to->group()->layoutItem()
                                                              : nullptr;
    const bool horizontal = location == Location_OnLeft || location == Location_OnRight;
    const int preferred_length = horizontal ? preferred_size.width() : preferred_size.height();
    const bool visible = option != StartHidden;

    insertItemRelativeTo(item, location, relative_item, preferred_length, visible);
    dock_widget->markOpen(visible);
}

void DropArea::addDockWidgetAsTab(DockWidget* dock_widget, Group* group)
{
    if (!dock_widget || !group)
        return;

    group->addDockWidget(dock_widget);
    if (group->layoutItem() && dock_widget->isOpen())
        group->refreshVisibility();
}

bool DropArea::isCentralItem(const Item* item) const
{
    for (const Item* current = item; current; current = current->parent()) {
        if (current == central_item_)
            return true;
    }
    return false;
}

Group* DropArea::groupAt(const QPoint& global_pos) const
{
    if (!root_item_)
        return nullptr;

    Group* hit = nullptr;
    const auto visit = [&](auto&& self, Item* item) -> void {
        if (!item || !item->isVisible())
            return;

        if (item->isContainer()) {
            for (Item* child : static_cast<ItemBoxContainer*>(item)->children())
                self(self, child);
            return;
        }

        auto* group = dynamic_cast<Group*>(item->guest());
        if (!group || !group->view())
            return;

        const QRect global_rect(global_origin_ + item->geometry().topLeft(), item->geometry().size());
        if (global_rect.contains(global_pos))
            hit = group;
    };
    visit(visit, root_item_);
    return hit;
}

void DropArea::insertItemRelativeTo(Item* item, Location location, Item* relative_to,
    int preferred_length, bool visible)
{
    if (!item)
        return;

    Item* target = relative_to ? relative_to : root_item_;
    if (!target) {
        root_item_ = item;
        item->setGeometry(geometry_);
        return;
    }

    const Qt::Orientation axis = (location == Location_OnLeft || location == Location_OnRight)
        ? Qt::Horizontal
        : Qt::Vertical;
    const bool before = location == Location_OnLeft || location == Location_OnTop;
    ItemBoxContainer* container = target->parent();

    if (!container) {
        // 根节点为叶子：包一层容器作为新根
        auto* wrapper = new ItemBoxContainer(axis);
        wrapper->setGeometry(target->geometry());
        wrapper->insertItem(0, target, 0, true);
        wrapper->insertItem(before ? 0 : 1, item, preferred_length, visible);
        root_item_ = wrapper;
        return;
    }

    if (container->orientation() == axis) {
        const int index = container->indexOfChild(target) + (before ? 0 : 1);
        container->insertItem(index, item, preferred_length, visible);
        return;
    }

    // 容器方向不同：在 target 原位包一层同向容器后再插入
    // 包层会改写 target 的占比（此时它已成为新容器的子项），须先记录其父级占比，
    // 并在替换回父容器前恢复到包装容器上，避免父容器重排时放大其他兄弟项
    const double target_percentage = target->percentage();
    const double target_stored_percentage = target->storedPercentage();
    auto* wrapper = new ItemBoxContainer(axis);
    wrapper->setGeometry(target->geometry());
    wrapper->insertItem(0, target, 0, true);
    wrapper->insertItem(before ? 0 : 1, item, preferred_length, visible);
    wrapper->setPercentage(target_percentage);
    wrapper->setStoredPercentage(target_stored_percentage);
    container->replaceChild(target, wrapper);
}

}
