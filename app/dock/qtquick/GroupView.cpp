/**
 * @file GroupView.cpp
 * @brief 分组视图的实现
 */

#include "GroupView.h"

#include "AreaItem.h"
#include "DockWidgetInstantiator.h"
#include "Platform.h"
#include "core/DockWidget.h"
#include "core/DragController.h"
#include "core/Draggable.h"
#include "core/FloatingWindow.h"
#include "core/Group.h"

#include <QQuickWindow>
#include <QVariant>

namespace dock::qtquick {

GroupView::GroupView(Group* group, QQuickItem* parent)
    : QQuickItem(parent)
    , group_(group)
{
    root_item_ = Platform::instance().createItemFromUrl(QUrl(QStringLiteral("qrc:/precess/dock/DockGroup.qml")));
    if (root_item_) {
        root_item_->setParentItem(this);
        root_item_->setProperty("groupView", QVariant::fromValue<QObject*>(this));
        root_item_->setSize(size());
        root_item_->setVisible(isVisible());
        content_area_ = root_item_->findChild<QQuickItem*>(QStringLiteral("contentArea"));

        connect(this, &QQuickItem::widthChanged, this, [this] {
            if (root_item_)
                root_item_->setWidth(width());
        });
        connect(this, &QQuickItem::heightChanged, this, [this] {
            if (root_item_)
                root_item_->setHeight(height());
        });
        connect(this, &QQuickItem::visibleChanged, this, [this] {
            if (root_item_)
                root_item_->setVisible(isVisible());
        });
        if (content_area_) {
            connect(content_area_, &QQuickItem::widthChanged, this, [this] { layoutGuest(); });
            connect(content_area_, &QQuickItem::heightChanged, this, [this] { layoutGuest(); });
        }
    }

    if (group_) {
        connect(group_, &Group::dockWidgetsChanged, this, &GroupView::syncFromGroup);
        connect(group_, &Group::currentDockWidgetChanged, this, [this](DockWidget*) {
            syncFromGroup();
        });
        connect(group_, &Group::titleChanged, this, [this](const QString&) {
            Q_EMIT groupChanged();
        });
    }

    syncFromGroup();
}

GroupView::~GroupView()
{
    if (drag_) {
        DragController::self().cancel();
        delete drag_;
        drag_ = nullptr;
    }

    // guest item 不属于本视图，销毁前摘出以免被级联删除
    if (shown_dock_) {
        if (QQuickItem* guest = Platform::instance().guestItem(shown_dock_))
            guest->setParentItem(nullptr);
    }

    // 从 Platform 缓存中注销，避免悬空视图被再次取用
    Platform::instance().forgetGroupView(group_, this);
}

Controller* GroupView::controller() const
{
    return group_;
}

void GroupView::syncFromGroup()
{
    if (!group_)
        return;

    updateGuest();
    Q_EMIT groupChanged();
}

void GroupView::updateGuest()
{
    DockWidget* current = group_ ? group_->currentDockWidget() : nullptr;
    if (current != shown_dock_) {
        if (shown_dock_) {
            if (QQuickItem* old_guest = Platform::instance().guestItem(shown_dock_))
                old_guest->setVisible(false);
        }

        shown_dock_ = current;
        if (shown_dock_) {
            connect(shown_dock_, &QObject::destroyed, this, [this] {
                shown_dock_ = nullptr;
            });
        }
    }

    // guest 可能在本视图创建之后才注册（如中央持久部件），每次同步都重试挂载
    if (shown_dock_ && content_area_) {
        if (QQuickItem* guest = Platform::instance().guestItem(shown_dock_)) {
            if (guest->parentItem() != content_area_)
                guest->setParentItem(content_area_);
        }
    }
    layoutGuest();
}

void GroupView::layoutGuest()
{
    if (!shown_dock_ || !content_area_)
        return;

    QQuickItem* guest = Platform::instance().guestItem(shown_dock_);
    if (!guest)
        return;

    guest->setPosition(QPointF(0, 0));
    guest->setSize(content_area_->size());
    // guest 自身可见性只随面板开关；分组隐藏由父项级联隐藏
    guest->setVisible(shown_dock_->isOpen());
}

void GroupView::setCurrentIndex(int index)
{
    if (group_)
        group_->setCurrentIndex(index);
}

void GroupView::closeGroup()
{
    if (!group_)
        return;

    const QList<DockWidget*> open = group_->openDockWidgets();
    for (DockWidget* dock_widget : open)
        dock_widget->close();

    if (AreaItem* area = areaItem())
        area->sync();
}

void GroupView::toggleFloat()
{
    if (!group_ || group_->isCentral())
        return;

    DragController::self().toggleFloating(group_);
    if (AreaItem* area = areaItem())
        area->sync();
}

void GroupView::beginDrag(const QPointF& global_pos)
{
    if (!group_)
        return;

    if (drag_) {
        DragController::self().cancel();
        delete drag_;
        drag_ = nullptr;
    }

    AreaItem* area = areaItem();
    FloatingWindow* floating_window = area ? area->floatingWindow() : nullptr;
    drag_ = new Draggable(this, group_, floating_window);
    DragController::self().onPress(drag_, global_pos.toPoint());
}

void GroupView::dragTo(const QPointF& global_pos)
{
    if (drag_)
        DragController::self().onMove(global_pos.toPoint());
}

void GroupView::endDrag(const QPointF& global_pos)
{
    if (!drag_)
        return;

    DragController::self().onRelease(global_pos.toPoint());
    delete drag_;
    drag_ = nullptr;

    if (AreaItem* area = areaItem())
        area->sync();
}

QString GroupView::title() const
{
    return group_ ? group_->title() : QString();
}

QStringList GroupView::tabTitles() const
{
    QStringList titles;
    if (!group_)
        return titles;
    const QList<DockWidget*> open = group_->openDockWidgets();
    for (DockWidget* dock_widget : open)
        titles.append(dock_widget->title());
    return titles;
}

int GroupView::tabCount() const
{
    return group_ ? group_->openDockWidgets().size() : 0;
}

int GroupView::currentIndex() const
{
    return group_ ? group_->currentIndex() : -1;
}

bool GroupView::isCentral() const
{
    return group_ && group_->isCentral();
}

bool GroupView::isFloating() const
{
    return group_ && group_->placeholderItem() != nullptr;
}

bool GroupView::titleBarVisible() const
{
    return group_ && !group_->isCentral();
}

void GroupView::setViewGeometry(const QRect& geometry)
{
    setPosition(geometry.topLeft());
    setSize(geometry.size());
}

QRect GroupView::viewGeometry() const
{
    return QRect(position().toPoint(), size().toSize());
}

void GroupView::setViewVisible(bool visible)
{
    setVisible(visible);
    // 分组重新显示时按当前面板状态恢复 guest 可见性（修复“重开面板空白”）
    if (visible)
        layoutGuest();
}

bool GroupView::isViewVisible() const
{
    return isVisible();
}

void GroupView::raiseView()
{
    setZ(z() + 1);
}

void GroupView::setViewCursor(Qt::CursorShape shape)
{
    setCursor(shape);
}

Qt::CursorShape GroupView::viewCursor() const
{
    return cursor().shape();
}

QPoint GroupView::viewGlobalPosition() const
{
    return mapToGlobal(QPointF(0, 0)).toPoint();
}

View* GroupView::createFloatingWindowView(Controller* controller)
{
    Q_UNUSED(controller);
    return nullptr;
}

AreaItem* GroupView::areaItem() const
{
    return qobject_cast<AreaItem*>(parentItem());
}

}
