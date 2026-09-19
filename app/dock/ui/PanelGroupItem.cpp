/**
 * @file PanelGroupItem.cpp
 * @brief 分组视图的实现
 */

#include "PanelGroupItem.h"

#include "DockAreaItem.h"
#include "DockPanelItem.h"
#include "DockRuntime.h"
#include "docking/DockPanel.h"
#include "docking/DragSession.h"
#include "docking/DragHandle.h"
#include "docking/DockWindow.h"
#include "docking/PanelGroup.h"

#include <QQuickWindow>
#include <QVariant>

namespace dock::ui {

PanelGroupItem::PanelGroupItem(PanelGroup* group, QQuickItem* parent)
    : QQuickItem(parent)
    , group_(group)
{
    root_item_ = DockRuntime::instance().createItemFromUrl(QUrl(QStringLiteral("qrc:/precess/dock/PanelGroup.qml")));
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
        connect(group_, &PanelGroup::panelsChanged, this, &PanelGroupItem::syncFromGroup);
        connect(group_, &PanelGroup::activePanelChanged, this, [this](DockPanel*) {
            syncFromGroup();
        });
        connect(group_, &PanelGroup::titleChanged, this, [this](const QString&) {
            Q_EMIT groupChanged();
        });
    }

    syncFromGroup();
}

PanelGroupItem::~PanelGroupItem()
{
    if (drag_) {
        DragSession::self().cancel();
        delete drag_;
        drag_ = nullptr;
    }

    // client item 不属于本视图，销毁前摘出以免被级联删除
    if (shown_dock_) {
        if (QQuickItem* client = DockRuntime::instance().panelContentItem(shown_dock_))
            client->setParentItem(nullptr);
    }

    // 从 DockRuntime 缓存中注销，避免悬空视图被再次取用
    DockRuntime::instance().forgetPanelGroupItem(group_, this);
}

DockObject* PanelGroupItem::dockObject() const
{
    return group_;
}

void PanelGroupItem::syncFromGroup()
{
    if (!group_)
        return;

    updateGuest();
    Q_EMIT groupChanged();
}

void PanelGroupItem::updateGuest()
{
    DockPanel* current = group_ ? group_->activePanel() : nullptr;
    if (current != shown_dock_) {
        if (shown_dock_) {
            if (QQuickItem* old_guest = DockRuntime::instance().panelContentItem(shown_dock_))
                old_guest->setVisible(false);
        }

        shown_dock_ = current;
        if (shown_dock_) {
            connect(shown_dock_, &QObject::destroyed, this, [this] {
                shown_dock_ = nullptr;
            });
        }
    }

    // client 可能在本视图创建之后才注册（如中央持久部件），每次同步都重试挂载
    if (shown_dock_ && content_area_) {
        if (QQuickItem* client = DockRuntime::instance().panelContentItem(shown_dock_)) {
            if (client->parentItem() != content_area_)
                client->setParentItem(content_area_);
        }
    }
    layoutGuest();
}

void PanelGroupItem::layoutGuest()
{
    if (!shown_dock_ || !content_area_)
        return;

    QQuickItem* client = DockRuntime::instance().panelContentItem(shown_dock_);
    if (!client)
        return;

    client->setPosition(QPointF(0, 0));
    client->setSize(content_area_->size());
    // client 自身可见性只随面板开关；分组隐藏由父项级联隐藏
    client->setVisible(shown_dock_->isShown());
}

void PanelGroupItem::activateTab(int index)
{
    if (group_)
        group_->setActiveIndex(index);
}

void PanelGroupItem::hideGroup()
{
    if (!group_)
        return;

    const QList<DockPanel*> open = group_->shownPanels();
    for (DockPanel* panel : open)
        panel->hidePanel();

    if (DockAreaItem* area = areaItem())
        area->sync();
}

void PanelGroupItem::toggleDetached()
{
    if (!group_ || group_->isCentral())
        return;

    DragSession::self().toggleDetached(group_);
    if (DockAreaItem* area = areaItem())
        area->sync();
}

void PanelGroupItem::beginGroupDrag(const QPointF& global_pos)
{
    if (!group_)
        return;

    if (drag_) {
        DragSession::self().cancel();
        delete drag_;
        drag_ = nullptr;
    }

    DockAreaItem* area = areaItem();
    DockWindow* window = area ? area->window() : nullptr;
    drag_ = new DragHandle(this, group_, window);
    DragSession::self().beginAt(drag_, global_pos.toPoint());
}

void PanelGroupItem::beginPanelDrag(int index, const QPointF& global_pos)
{
    if (!group_)
        return;

    const QList<DockPanel*> open = group_->shownPanels();
    if (index < 0 || index >= open.size())
        return;

    DockPanel* panel = open.at(index);
    group_->setActivePanel(panel);

    if (drag_) {
        DragSession::self().cancel();
        delete drag_;
        drag_ = nullptr;
    }

    DockAreaItem* area = areaItem();
    DockWindow* window = area ? area->window() : nullptr;
    drag_ = new DragHandle(this, group_, window, panel);
    DragSession::self().beginAt(drag_, global_pos.toPoint());
}

void PanelGroupItem::dragTo(const QPointF& global_pos)
{
    if (drag_)
        DragSession::self().updateAt(global_pos.toPoint());
}

void PanelGroupItem::endDrag(const QPointF& global_pos)
{
    if (!drag_)
        return;

    DragSession::self().endAt(global_pos.toPoint());
    delete drag_;
    drag_ = nullptr;

    if (DockAreaItem* area = areaItem())
        area->sync();
}

QString PanelGroupItem::title() const
{
    return group_ ? group_->title() : QString();
}

QStringList PanelGroupItem::tabNames() const
{
    QStringList titles;
    if (!group_)
        return titles;
    const QList<DockPanel*> open = group_->shownPanels();
    for (DockPanel* panel : open)
        titles.append(panel->title());
    return titles;
}

int PanelGroupItem::tabCount() const
{
    return group_ ? group_->shownPanels().size() : 0;
}

int PanelGroupItem::activeIndex() const
{
    return group_ ? group_->activeIndex() : -1;
}

bool PanelGroupItem::isCentral() const
{
    return group_ && group_->isCentral();
}

bool PanelGroupItem::isDetached() const
{
    return group_ && group_->vacancy() != nullptr;
}

bool PanelGroupItem::hasTitleBar() const
{
    return group_ && !group_->isCentral();
}

void PanelGroupItem::applyFrame(const QRect& geometry)
{
    setPosition(geometry.topLeft());
    setSize(geometry.size());
}

QRect PanelGroupItem::frame() const
{
    return QRect(position().toPoint(), size().toSize());
}

void PanelGroupItem::applyVisibility(bool visible)
{
    setVisible(visible);
    // 分组重新显示时按当前面板状态恢复 client 可见性（修复“重开面板空白”）
    if (visible)
        layoutGuest();
}

bool PanelGroupItem::isShown() const
{
    return isVisible();
}

void PanelGroupItem::bringToFront()
{
    setZ(z() + 1);
}

void PanelGroupItem::setCursorShape(Qt::CursorShape shape)
{
    setCursor(shape);
}

Qt::CursorShape PanelGroupItem::cursorShape() const
{
    return cursor().shape();
}

QPoint PanelGroupItem::globalOrigin() const
{
    return mapToGlobal(QPointF(0, 0)).toPoint();
}

DockView* PanelGroupItem::createDockWindow(DockObject* controller)
{
    Q_UNUSED(controller);
    return nullptr;
}

DockAreaItem* PanelGroupItem::areaItem() const
{
    return qobject_cast<DockAreaItem*>(parentItem());
}

}
