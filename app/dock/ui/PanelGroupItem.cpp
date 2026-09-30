/**
 * @file PanelGroupItem.cpp
 * @brief 分组视图的实现
 */

#include "PanelGroupItem.h"

#include "DockAreaItem.h"
#include "DockPanelItem.h"
#include "DockRuntime.h"
#include "docking/DockPanel.h"
#include "docking/DockCatalog.h"
#include "docking/DockHost.h"
#include "docking/DragSession.h"
#include "docking/DragHandle.h"
#include "docking/DockMetrics.h"
#include "docking/DockWindow.h"
#include "docking/PanelGroup.h"

#include <QVariant>

#include <algorithm>

namespace dock::ui {

PanelGroupItem::PanelGroupItem(PanelGroup* group, QQuickItem* parent)
    : QQuickItem(parent)
    , group_(group)
{
    root_item_ = DockRuntime::instance().createItemFromUrl(QUrl(QStringLiteral("qrc:/precess/dock/PanelGroup.qml")));
    if (root_item_) {
        root_item_->setParentItem(this);
        // QQuickItem::setParentItem 不改 QObject 父子关系：显式接管所有权并让 findChild 可用
        root_item_->setParent(this);
        root_item_->setProperty("groupView", QVariant::fromValue<QObject*>(this));
        root_item_->setSize(size());
        root_item_->setVisible(isVisible());
        content_area_ = root_item_->findChild<QQuickItem*>(QStringLiteral("contentArea"));
        tab_bar_ = root_item_->findChild<QQuickItem*>(QStringLiteral("tabBar"));
        title_bar_ = root_item_->findChild<QQuickItem*>(QStringLiteral("titleBar"));

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
        // 分组被回收（如合并后空组回收）时视图进入安全空态
        connect(group_, &QObject::destroyed, this, [this] { group_ = nullptr; });
        connect(group_, &PanelGroup::panelsChanged, this, &PanelGroupItem::syncFromGroup);
        connect(group_, &PanelGroup::activePanelChanged, this, [this](DockPanel*) {
            // 仅切换内容与下标：不重建标签模型，避免按下未激活标签时销毁其委托
            updateGuest();
            syncTabIndex();
            // 同步签名缓存：后续区域 sync 不因激活切换重复通知 QML
            last_signature_ = stateSignature();
            Q_EMIT activeTabChanged();
        });
        connect(group_, &PanelGroup::titleChanged, this, [this](const QString&) {
            Q_EMIT groupChanged();
        });
    }

    // 会话（可能由全局中继结束，不经过 endDrag）回到空闲后释放遗留拖拽句柄
    connect(&DragSession::self(), &DragSession::phaseChanged, this,
        [this](DragSession::Phase phase) {
            if (phase == DragSession::Phase::Idle && drag_
                && DragSession::self().handle() == nullptr) {
                delete drag_;
                drag_ = nullptr;
            }
        });

    syncFromGroup();
}

PanelGroupItem::~PanelGroupItem()
{
    if (drag_) {
        DragSession::self().cancel();
        delete drag_;
        drag_ = nullptr;
    }

    // client item 不属于本视图，销毁前交还面板宿主（面板声明所在窗口）；
    // 若已被新视图接管则不再打扰
    if (shown_dock_) {
        if (QQuickItem* client = DockRuntime::instance().panelContentItem(shown_dock_)) {
            if (client->parentItem() == content_area_) {
                client->setVisible(false);
                // 宿主缺失时退回无父项（与旧行为一致），避免留在即将销毁的内容区上
                client->setParentItem(DockRuntime::instance().panelHome(shown_dock_));
            }
        }
    }

    // 从 DockRuntime 缓存中注销，避免悬空视图被再次取用；
    // 仅在本视图仍是登记视图时清除分组引用，避免组已进入销毁流程时反向访问
    if (group_ && DockRuntime::instance().existingPanelGroupItem(group_) == this)
        group_->setView(nullptr);
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

    // client 可能延迟注册（如中央持久部件）：每次同步都重试挂载
    updateGuest();

    // 状态未变时不通知 QML：避免区域反复 sync 造成标签模型重建
    const QVariantList signature = stateSignature();
    if (signature == last_signature_) {
        syncTabIndex();
        return;
    }
    last_signature_ = signature;

    // 模型重建期间 TabBar 可能重置下标并回灌激活请求，用标记抑制
    updating_tabs_ = true;
    Q_EMIT groupChanged();
    updating_tabs_ = false;
    // 选项卡模型重建后校正 TabBar 当前下标（rebuild 期间 TabBar 可能重置下标）
    syncTabIndex();
}

QVariantList PanelGroupItem::stateSignature() const
{
    QVariantList signature;
    signature << tabNames()
              << activeIndex()
              << title()
              << isCentral()
              << isDetached()
              << hasTitleBar()
              << isClosable()
              << isFloatable();
    return signature;
}

void PanelGroupItem::syncTabIndex()
{
    if (tab_bar_)
        tab_bar_->setProperty("currentIndex", activeIndex());
}

void PanelGroupItem::updateGuest()
{
    DockPanel* current = group_ ? group_->activePanel() : nullptr;
    if (current != shown_dock_) {
        if (shown_dock_) {
            // 断开旧面板的连接：避免切换回同一面板时重复累积，也避免其析构误清当前面板
            disconnect(shown_dock_, &QObject::destroyed, this, nullptr);
            if (QQuickItem* old_guest = DockRuntime::instance().panelContentItem(shown_dock_))
                old_guest->setVisible(false);
        }

        shown_dock_ = current;
        if (shown_dock_) {
            DockPanel* panel = shown_dock_;
            connect(panel, &QObject::destroyed, this, [this, panel] {
                if (shown_dock_ == panel)
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

    // 与 hidePanelAt 一致的能力门控：中央持久分组不可关闭，组内能力按交集判定
    if (group_->isCentral() || !group_->features().testFlag(DockPanel::Feature::Closable))
        return;

    const QList<DockPanel*> open = group_->shownPanels();
    for (DockPanel* panel : open)
        panel->hidePanel();

    if (DockAreaItem* area = areaItem())
        area->sync();
}

void PanelGroupItem::hidePanelAt(int index)
{
    if (!group_)
        return;

    const QList<DockPanel*> open = group_->shownPanels();
    if (index < 0 || index >= open.size())
        return;

    DockPanel* panel = open.at(index);
    if (panel->isCentral() || !panel->hasFeature(DockPanel::Feature::Closable))
        return;

    panel->hidePanel();

    if (DockAreaItem* area = areaItem())
        area->sync();
}

void PanelGroupItem::hideOthers(int index)
{
    if (!group_)
        return;

    const QList<DockPanel*> open = group_->shownPanels();
    if (index < 0 || index >= open.size())
        return;

    group_->hideOthers(open.at(index));

    if (DockAreaItem* area = areaItem())
        area->sync();
}

void PanelGroupItem::hideOtherGroups()
{
    if (!group_)
        return;

    if (DockHost* host = DockCatalog::self().host())
        host->hideOtherGroups(group_);

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

    // 清理上次拖拽遗留句柄（会话由全局中继结束时不会经过 endDrag）
    if (drag_) {
        DragSession::self().cancel();
        delete drag_;
        drag_ = nullptr;
    }

    // 中央持久分组不可拖出（其节点受区域保护，拖出会破坏布局树）
    if (group_->isCentral())
        return;

    // 能力门控：不可移动的分组不进入拖拽
    if (!group_->features().testFlag(DockPanel::Feature::Movable))
        return;

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

    // 清理上次拖拽遗留句柄（会话由全局中继结束时不会经过 endDrag），
    // 避免门控拦截后残留句柄被后续 dragTo 误用
    if (drag_) {
        DragSession::self().cancel();
        delete drag_;
        drag_ = nullptr;
    }

    // 中央持久面板不可拖出（点击仅激活其标签）
    if (panel->isCentral())
        return;

    // 能力门控：不可移动的面板不进入拖拽/重排
    if (!panel->hasFeature(DockPanel::Feature::Movable))
        return;

    // 延迟进入拖拽会话：先记录按下信息，由 dragTo 判别组内重排或浮动
    press_global_ = global_pos;
    reorder_from_ = index;
    reorder_marker_index_ = -1;
    reordering_ = false;

    DockAreaItem* area = areaItem();
    DockWindow* window = area ? area->window() : nullptr;
    drag_ = new DragHandle(this, group_, window, panel);
}

void PanelGroupItem::dragTo(const QPointF& global_pos)
{
    if (!drag_)
        return;

    DragSession& session = DragSession::self();
    if (session.phase() != DragSession::Phase::Idle) {
        session.updateAt(global_pos.toPoint());
        return;
    }

    // 起步阈值与 DragSession 统一（曼哈顿距离）；轴向判别仍按单轴分量：
    // 纵向达阈值（或分组不可重排）转浮动，否则横向进入组内重排预览
    const qreal dy = qAbs(global_pos.y() - press_global_.y());
    if (!DockMetrics::exceedsDragThreshold((global_pos - press_global_).toPoint()))
        return;

    const bool multi = group_ && group_->shownPanels().size() > 1;

    // 纵向超过阈值（或分组无法重排）：转为浮动拖拽
    if (dy >= DockMetrics::kStartDragDistance || !multi) {
        clearReorderPreview();
        session.beginAt(drag_, press_global_.toPoint());
        session.updateAt(global_pos.toPoint());
        return;
    }

    // 横向移动：组内重排预览（不修改模型，释放时提交）
    const int insert_index = tabInsertIndexAt(global_pos.toPoint());
    if (insert_index < 0) {
        clearReorderPreview();
        return;
    }

    const int final_index = insert_index > reorder_from_ ? insert_index - 1 : insert_index;
    const int marker_index = (final_index == reorder_from_) ? -1 : insert_index;
    if (marker_index == reorder_marker_index_)
        return;

    reorder_marker_index_ = marker_index;
    reordering_ = marker_index >= 0;
    Q_EMIT reorderChanged();
}

void PanelGroupItem::endDrag(const QPointF& global_pos)
{
    if (!drag_)
        return;

    DragSession& session = DragSession::self();
    if (session.phase() != DragSession::Phase::Idle) {
        session.endAt(global_pos.toPoint());
    } else if (reordering_ && group_ && reorder_from_ >= 0
        && reorder_marker_index_ >= 0) {
        // 提交组内重排
        group_->movePanel(reorder_from_, reorder_marker_index_);
    }

    clearReorderPreview();
    reorder_from_ = -1;
    delete drag_;
    drag_ = nullptr;

    if (DockAreaItem* area = areaItem())
        area->sync();
}

void PanelGroupItem::clearReorderPreview()
{
    if (!reordering_ && reorder_marker_index_ < 0)
        return;
    reordering_ = false;
    reorder_marker_index_ = -1;
    Q_EMIT reorderChanged();
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
    // 与 DragSession 的浮动判定一致：含停靠在浮窗内的次级分组与单标签浮停
    return group_ && DragSession::isFloating(group_);
}

bool PanelGroupItem::hasTitleBar() const
{
    return group_ && !group_->isCentral();
}

bool PanelGroupItem::isClosable() const
{
    return group_ && group_->features().testFlag(DockPanel::Feature::Closable);
}

bool PanelGroupItem::isFloatable() const
{
    return group_ && group_->features().testFlag(DockPanel::Feature::Floatable);
}

int PanelGroupItem::reorderMarkerX() const
{
    if (!reordering_)
        return -1;
    const QRect marker = tabInsertMarkerRect(reorder_marker_index_);
    return marker.isNull() ? -1 : marker.x();
}

QQuickItem* PanelGroupItem::tabItem(int index) const
{
    if (!tab_bar_ || index < 0)
        return nullptr;

    QQuickItem* item = nullptr;
    if (!QMetaObject::invokeMethod(tab_bar_, "itemAt", Q_RETURN_ARG(QQuickItem*, item),
            Q_ARG(int, index)))
        return nullptr;
    return item;
}

int PanelGroupItem::tabInsertIndexAt(const QPoint& global_pos) const
{
    if (!group_ || group_->isCentral())
        return -1;

    // 标签栏：落在标签上时按左/右半决定插到该标签前或后，尾部追加
    if (tab_bar_ && tab_bar_->isVisible() && tab_bar_->height() > 0) {
        const QPointF local = tab_bar_->mapFromGlobal(global_pos);
        if (local.x() >= 0 && local.y() >= 0
            && local.x() <= tab_bar_->width() && local.y() <= tab_bar_->height()) {
            const int count = group_->shownPanels().size();
            for (int i = 0; i < count; ++i) {
                QQuickItem* item = tabItem(i);
                if (!item)
                    continue;
                const QPointF item_pos = item->mapToItem(tab_bar_, QPointF(0, 0));
                const qreal left = item_pos.x();
                const qreal right = left + item->width();
                if (local.x() < left)
                    return i;
                if (local.x() <= right)
                    return local.x() < (left + right) / 2.0 ? i : i + 1;
            }
            return count;
        }
    }

    // 标题栏：按追加处理（与中心合并一致）
    if (title_bar_ && title_bar_->isVisible() && title_bar_->height() > 0) {
        const QPointF local = title_bar_->mapFromGlobal(global_pos);
        if (local.x() >= 0 && local.y() >= 0
            && local.x() <= title_bar_->width() && local.y() <= title_bar_->height())
            return group_->shownPanels().size();
    }

    return -1;
}

QRect PanelGroupItem::tabInsertMarkerRect(int index) const
{
    if (!group_ || !tab_bar_ || !tab_bar_->isVisible() || tab_bar_->height() <= 0)
        return {};

    const int count = group_->shownPanels().size();
    if (count <= 1)
        return {};

    index = std::clamp(index, 0, count);
    const QPointF bar_pos = tab_bar_->mapToItem(this, QPointF(0, 0));

    qreal x = bar_pos.x();
    if (index >= count) {
        QQuickItem* last = tabItem(count - 1);
        x = last ? last->mapToItem(this, QPointF(last->width(), 0)).x()
                 : bar_pos.x() + tab_bar_->width();
    } else if (QQuickItem* item = tabItem(index)) {
        x = item->mapToItem(this, QPointF(0, 0)).x();
    }

    return QRect(QPoint(qRound(x) - 1, qRound(bar_pos.y())),
        QSize(3, qRound(tab_bar_->height())));
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
    // 层 z 受控：分组视图恒在分组层，避免无界自增并盖过分隔条
    setZ(DockMetrics::kGroupLayerZ);
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
