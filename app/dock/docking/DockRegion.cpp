/**
 * @file DockRegion.cpp
 * @brief 停靠区域的实现
 */

#include "DockRegion.h"

#include "DockPanel.h"
#include "PanelGroup.h"
#include "DockView.h"
#include "tree/LayoutNode.h"
#include "tree/BoxNode.h"

#include <QSet>

namespace dock {

DockRegion::DockRegion(QObject* parent)
    : DockObject(Kind::None, parent)
{
}

DockRegion::~DockRegion()
{
    delete root_item_;
}

void DockRegion::setRootNode(LayoutNode* item)
{
    if (root_item_ == item)
        return;

    delete root_item_;
    root_item_ = item;
    if (root_item_)
        root_item_->setGeometry(geometry_);
}

LayoutNode* DockRegion::takeRootNode()
{
    LayoutNode* item = root_item_;
    root_item_ = nullptr;
    return item;
}

void DockRegion::setGeometry(const QRect& geometry)
{
    geometry_ = geometry;
    if (root_item_)
        root_item_->setGeometry(geometry);
}

void DockRegion::placePanel(DockPanel* panel, DockEdge edge,
    DockPanel* relative_to, const QSize& preferred_size, PanelLaunch launch)
{
    if (!panel)
        return;

    auto* group = new PanelGroup(this);
    group->addPanel(panel);

    auto* item = new LayoutNode(group);
    item->setId(panel->uniqueName());
    group->setNode(item);

    LayoutNode* relative_item = relative_to && relative_to->group() ? relative_to->group()->node()
                                                              : nullptr;
    const bool horizontal = edge == DockEdge::Left || edge == DockEdge::Right;
    const int preferred_length = horizontal ? preferred_size.width() : preferred_size.height();
    const bool visible = launch != PanelLaunch::Hidden;

    insertRelative(item, edge, relative_item, preferred_length, visible);
    panel->applyShown(visible);
}

void DockRegion::stackPanel(DockPanel* panel, PanelGroup* group)
{
    if (!panel || !group)
        return;

    group->addPanel(panel);
    if (group->node() && panel->isShown())
        group->syncVisibility();
}

LayoutNode* DockRegion::extractGroupForWindow(PanelGroup* group)
{
    if (!group)
        return nullptr;

    LayoutNode* item = group->node();
    if (!item || item == root_item_)
        return nullptr; // 根节点（仅剩该分组时）不支持，主窗口含中央部件时不会出现

    if (isCentralNode(item))
        return nullptr; // 中央持久节点不可摘出

    BoxNode* parent = item->parent();
    if (!parent)
        return nullptr;

    const int index = parent->indexOfNode(item);
    const double share = item->share();

    auto* placeholder = new LayoutNode(nullptr);
    placeholder->setId(item->id());
    parent->insertNode(index, placeholder, 0, false);
    placeholder->setRememberedShare(share);
    group->setVacancy(placeholder);

    parent->detachNode(item, false);
    item->setParent(nullptr);
    item->setVisible(true);
    return item;
}

bool DockRegion::restoreGroupFromWindow(PanelGroup* group)
{
    if (!group)
        return false;

    LayoutNode* placeholder = group->vacancy();
    LayoutNode* item = group->node();
    if (!placeholder || !item)
        return false;

    if (isCentralNode(item))
        return false; // 防御：中央持久节点不应有拆出占位

    group->setVacancy(nullptr);
    item->setShare(placeholder->rememberedShare());
    item->setRememberedShare(placeholder->rememberedShare());

    BoxNode* parent = placeholder->parent();
    if (parent) {
        parent->replaceNode(placeholder, item);
        delete placeholder;
        item->setVisible(true); // 触发父容器按占位占比让位
    } else if (placeholder == root_item_) {
        root_item_ = item;
        delete placeholder;
        item->setShare(1.0);
        item->setGeometry(geometry_);
        item->setVisible(true);
    } else {
        delete placeholder;
        return false;
    }

    group->setDetached(false);
    return true;
}

void DockRegion::discardGroupVacancy(PanelGroup* group)
{
    if (!group)
        return;

    LayoutNode* placeholder = group->vacancy();
    if (!placeholder)
        return;

    group->setVacancy(nullptr);
    if (placeholder == root_item_) {
        root_item_ = nullptr;
    } else if (BoxNode* parent = placeholder->parent()) {
        parent->detachNode(placeholder, false);
    }
    delete placeholder;
}

bool DockRegion::extractGroupNode(PanelGroup* group)
{
    if (!group)
        return false;

    LayoutNode* item = group->node();
    if (!item)
        return false;

    if (item == root_item_) {
        root_item_ = nullptr;
        return true;
    }

    BoxNode* parent = item->parent();
    if (!parent)
        return false;

    parent->detachNode(item, false);
    item->setParent(nullptr);
    item->setVisible(true);
    return true;
}

bool DockRegion::attachGroup(PanelGroup* group, DropZone location, PanelGroup* target_group,
    const QSize& preferred_size)
{
    if (!group || !group->node() || location == DropZone::None
        || location == DropZone::Merge)
        return false;

    const bool outer = isOuterZone(location);
    const bool horizontal = location == DropZone::InnerLeft || location == DropZone::InnerRight
        || location == DropZone::OuterLeft || location == DropZone::OuterRight;
    const bool before = location == DropZone::InnerLeft || location == DropZone::InnerTop
        || location == DropZone::OuterLeft || location == DropZone::OuterTop;
    const DockEdge dock_edge = before
        ? (horizontal ? DockEdge::Left : DockEdge::Top)
        : (horizontal ? DockEdge::Right : DockEdge::Bottom);
    const int preferred = horizontal ? preferred_size.width() : preferred_size.height();

    LayoutNode* relative_item = nullptr;
    if (!outer && target_group)
        relative_item = target_group->node();

    insertRelative(group->node(), dock_edge, relative_item, preferred, true);
    group->setDetached(false);
    return true;
}

bool DockRegion::isCentralNode(const LayoutNode* item) const
{
    for (const LayoutNode* current = item; current; current = current->parent()) {
        if (current == central_item_)
            return true;
    }
    return false;
}

PanelGroup* DockRegion::groupAt(const QPoint& global_pos) const
{
    if (!root_item_)
        return nullptr;

    PanelGroup* hit = nullptr;
    const auto visit = [&](auto&& self, LayoutNode* item) -> void {
        if (!item || !item->isVisible())
            return;

        if (item->isContainer()) {
            for (LayoutNode* child : static_cast<BoxNode*>(item)->children())
                self(self, child);
            return;
        }

        auto* group = dynamic_cast<PanelGroup*>(item->client());
        if (!group)
            return;

        const QRect global_rect(global_origin_ + item->geometry().topLeft(), item->geometry().size());
        if (global_rect.contains(global_pos))
            hit = group;
    };
    visit(visit, root_item_);
    return hit;
}

QList<PanelGroup*> DockRegion::groups() const
{
    QList<PanelGroup*> result;
    const auto visit = [&](auto&& self, LayoutNode* item) -> void {
        if (!item)
            return;

        if (item->isContainer()) {
            for (LayoutNode* child : static_cast<BoxNode*>(item)->children())
                self(self, child);
            return;
        }

        if (auto* group = dynamic_cast<PanelGroup*>(item->client()))
            result.append(group);
    };
    visit(visit, root_item_);
    return result;
}

void DockRegion::insertRelative(LayoutNode* item, DockEdge edge, LayoutNode* relative_to,
    int preferred_length, bool visible)
{
    if (!item)
        return;

    LayoutNode* target = relative_to ? relative_to : root_item_;
    if (!target) {
        root_item_ = item;
        item->setGeometry(geometry_);
        return;
    }

    const Qt::Orientation axis = (edge == DockEdge::Left || edge == DockEdge::Right)
        ? Qt::Horizontal
        : Qt::Vertical;
    const bool before = edge == DockEdge::Left || edge == DockEdge::Top;
    BoxNode* container = target->parent();

    if (!container) {
        // 根节点为叶子：包一层容器作为新根
        auto* wrapper = new BoxNode(axis);
        wrapper->setGeometry(target->geometry());
        wrapper->insertNode(0, target, 0, true);
        wrapper->insertNode(before ? 0 : 1, item, preferred_length, visible);
        root_item_ = wrapper;
        return;
    }

    if (container->orientation() == axis) {
        const int index = container->indexOfNode(target) + (before ? 0 : 1);
        container->insertNode(index, item, preferred_length, visible);
        return;
    }

    // 容器方向不同：在 target 原位包一层同向容器后再插入
    // 包层会改写 target 的占比（此时它已成为新容器的子项），须先记录其父级占比，
    // 并在替换回父容器前恢复到包装容器上，避免父容器重排时放大其他兄弟项
    const double target_percentage = target->share();
    const double target_stored_percentage = target->rememberedShare();
    auto* wrapper = new BoxNode(axis);
    wrapper->setGeometry(target->geometry());
    wrapper->insertNode(0, target, 0, true);
    wrapper->insertNode(before ? 0 : 1, item, preferred_length, visible);
    wrapper->setShare(target_percentage);
    wrapper->setRememberedShare(target_stored_percentage);
    container->replaceNode(target, wrapper);
}

#ifdef QT_DEBUG
void DockRegion::validateTree() const
{
    QSet<const LayoutNode*> visited;
    const auto walk = [&visited](auto&& self, const LayoutNode* item) -> void {
        if (!item)
            return;
        Q_ASSERT_X(!visited.contains(item), "DockRegion", "layout tree cycle detected");
        visited.insert(item);
        if (!item->isContainer())
            return;

        const auto* container = static_cast<const BoxNode*>(item);
        for (const LayoutNode* child : container->children()) {
            Q_ASSERT_X(child != nullptr, "DockRegion", "layout tree null child");
            Q_ASSERT_X(child->parent() == container, "DockRegion", "layout tree parent mismatch");
            self(self, child);
        }
    };
    walk(walk, root_item_);
}
#endif

}
