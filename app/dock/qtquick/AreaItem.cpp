/**
 * @file AreaItem.cpp
 * @brief 布局区域视图的实现
 */

#include "AreaItem.h"

#include "GroupView.h"
#include "IndicatorsOverlayWindow.h"
#include "Platform.h"
#include "SeparatorView.h"
#include "core/ClassicIndicators.h"
#include "core/DragController.h"
#include "core/DropArea.h"
#include "core/FloatingWindow.h"
#include "core/Group.h"
#include "engine/Item.h"
#include "engine/ItemBoxContainer.h"
#include "engine/Separator.h"

namespace dock::qtquick {

AreaItem::AreaItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    // 拖拽结束（落点生效或取消）后布局可能已变化，统一重同步
    connect(&DragController::self(), &DragController::stateChanged, this,
        [this](DragController::State state) {
            if (state == DragController::State::Idle)
                sync();
            updateIndicators();
        });
    // 浮动/回停等命令式布局变化
    connect(&DragController::self(), &DragController::layoutChanged, this, &AreaItem::sync);
    connect(&DragController::self(), &DragController::hoverChanged, this, &AreaItem::updateIndicators);
}

AreaItem::~AreaItem()
{
    if (auto* overlay = Platform::instance().indicatorsOverlay())
        overlay->clear(this);

    // 分组视图所有权归 Platform：析构前摘出，避免随本项被级联删除
    for (QQuickItem* child : childItems()) {
        if (auto* view = qobject_cast<GroupView*>(child))
            view->setParentItem(nullptr);
    }
}

void AreaItem::setDropArea(DropArea* drop_area)
{
    if (drop_area_ == drop_area)
        return;

    if (drop_area_)
        disconnect(drop_area_, nullptr, this, nullptr);

    clearSeparatorViews();

    drop_area_ = drop_area;
    if (!drop_area_)
        return;

    // 核心层先于视图销毁时（浮动窗口关闭），立即解除引用，避免悬空解引用
    connect(drop_area_, &QObject::destroyed, this, [this] {
        drop_area_ = nullptr;
        clearSeparatorViews();
        if (auto* overlay = Platform::instance().indicatorsOverlay())
            overlay->clear(this);
    });
    sync();
}

void AreaItem::clearSeparatorViews()
{
    qDeleteAll(separator_views_);
    separator_views_.clear();
}

void AreaItem::sync()
{
    if (!drop_area_ || syncing_)
        return;

    syncing_ = true;

    // 分组视图：按布局树叶子节点同步几何与可见性
    QSet<Group*> groups;
    collectGroups(drop_area_->rootItem(), groups);
    for (Group* group : groups) {
        GroupView* view = Platform::instance().groupView(group);
        if (!view)
            continue;

        if (view->parentItem() != this)
            view->setParentItem(this);

        Item* item = group->layoutItem();
        if (item) {
            view->setPosition(item->geometry().topLeft());
            view->setSize(item->geometry().size());
            view->setVisible(item->isVisible());
        }
        // 显式 z 序：分组在下、分隔条在上，避免残留视图互相遮挡
        view->setZ(1);
        // guest 可能延迟注册（如中央持久部件）：每次同步刷新挂载与可见性
        view->syncFromGroup();
    }

    // 分隔条视图：创建/更新/回收
    QSet<Separator*> separators;
    collectSeparators(drop_area_->rootItem(), separators);

    for (auto it = separator_views_.begin(); it != separator_views_.end();) {
        if (!separators.contains(it.key())) {
            delete it.value();
            it = separator_views_.erase(it);
        } else {
            ++it;
        }
    }

    for (Separator* separator : separators) {
        SeparatorView* view = separator_views_.value(separator, nullptr);
        if (!view) {
            view = new SeparatorView(this);
            view->setSeparator(separator, this);
            separator_views_.insert(separator, view);
        }
        view->setPosition(separator->geometry().topLeft());
        view->setSize(separator->geometry().size());
        view->setVisible(true);
        view->setZ(2);
    }

    updateIndicators();
    syncing_ = false;
}

void AreaItem::collectGroups(Item* item, QSet<Group*>& groups) const
{
    if (!item)
        return;

    if (item->isContainer()) {
        const auto* container = static_cast<ItemBoxContainer*>(item);
        for (Item* child : container->children())
            collectGroups(child, groups);
        return;
    }

    if (auto* group = dynamic_cast<Group*>(item->guest()))
        groups.insert(group);
}

void AreaItem::collectSeparators(Item* item, QSet<Separator*>& separators) const
{
    if (!item || !item->isContainer())
        return;

    const auto* container = static_cast<ItemBoxContainer*>(item);
    for (Separator* separator : container->separators())
        separators.insert(separator);
    for (Item* child : container->children())
        collectSeparators(child, separators);
}

void AreaItem::updateIndicators()
{
    IndicatorsOverlayWindow* overlay = Platform::instance().indicatorsOverlay();
    if (!overlay)
        return;

    DragController& drag = DragController::self();
    if (drag.state() != DragController::State::Dragging || drag.hoveredArea() != drop_area_
        || drag.hoveredLocation() == DropLocation_None) {
        overlay->clear(this);
        return;
    }

    const QRect area_rect(QPoint(0, 0), size().toSize());
    QRect group_rect = area_rect;
    if (drag.hoveredGroup() && drag.hoveredGroup()->layoutItem())
        group_rect = drag.hoveredGroup()->layoutItem()->geometry();

    const QList<ClassicIndicators::Indicator> candidates
        = ClassicIndicators::indicatorRects(area_rect, group_rect);
    for (const ClassicIndicators::Indicator& indicator : candidates) {
        if (indicator.location == drag.hoveredLocation()) {
            const QRect area_global(drop_area_->globalOrigin(), area_rect.size());
            const QRect highlight_global(area_global.topLeft() + indicator.rect.topLeft(),
                indicator.rect.size());
            overlay->showHighlight(highlight_global, area_global, this);
            return;
        }
    }
    overlay->clear(this);
}

}
