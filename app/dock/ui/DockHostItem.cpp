/**
 * @file DockHostItem.cpp
 * @brief 主停靠区域视图的实现
 */

#include "DockHostItem.h"

#include "DockAreaItem.h"
#include "DockPanelItem.h"
#include "DockWindowItem.h"
#include "PanelGroupItem.h"
#include "PanelContentView.h"
#include "DockRuntime.h"
#include "docking/DockPanel.h"
#include "docking/DockRegion.h"
#include "docking/DockWindow.h"
#include "docking/PanelGroup.h"
#include "docking/DockHost.h"
#include "tree/LayoutNode.h"

#include <QQuickWindow>

namespace dock::ui {

DockHostItem::DockHostItem(QQuickItem* parent)
    : QQuickItem(parent)
{
}

DockHostItem::~DockHostItem() = default;

void DockHostItem::setUniqueName(const QString& unique_name)
{
    if (unique_name_ == unique_name)
        return;
    unique_name_ = unique_name;
    Q_EMIT uniqueNameChanged();
}

void DockHostItem::setCentralItemFile(const QUrl& file)
{
    if (central_file_ == file)
        return;
    central_file_ = file;
    Q_EMIT centralItemFileChanged();
}

void DockHostItem::componentComplete()
{
    QQuickItem::componentComplete();

    if (unique_name_.isEmpty())
        unique_name_ = QStringLiteral("DockHost");

    host_ = new dock::DockHost(unique_name_, this);
    host_->setView(this);

    area_item_ = new DockAreaItem(this);
    area_item_->setSize(size());
    area_item_->setRegion(host_->region());

    watchWindow();
    loadCentralItem();
    updateAreaGeometry();
}

void DockHostItem::geometryChange(const QRectF& new_geometry, const QRectF& old_geometry)
{
    QQuickItem::geometryChange(new_geometry, old_geometry);
    if (area_item_)
        area_item_->setSize(new_geometry.size());
    updateAreaGeometry();
}

void DockHostItem::updateAreaGeometry()
{
    if (!host_)
        return;

    const QPoint origin = mapToGlobal(QPointF(0, 0)).toPoint();
    host_->region()->setGlobalOrigin(origin);
    host_->setFrame(QRect(QPoint(0, 0), size().toSize()));
    if (area_item_)
        area_item_->sync();
}

void DockHostItem::watchWindow()
{
    if (window_watched_)
        return;

    QQuickWindow* host = window();
    if (!host)
        return;

    connect(host, &QWindow::xChanged, this, [this] { updateAreaGeometry(); });
    connect(host, &QWindow::yChanged, this, [this] { updateAreaGeometry(); });
    window_watched_ = true;
}

void DockHostItem::loadCentralItem()
{
    if (central_file_.isEmpty() || !host_ || !host_->centralPanel())
        return;

    QQuickItem* item = DockRuntime::instance().createItemFromUrl(central_file_);
    if (!item)
        return;

    DockRuntime::instance().registerPanelContent(host_->centralPanel(), item);
    central_view_ = std::make_unique<PanelContentView>(host_->centralPanel(), item);
    host_->setCentralContentView(central_view_.get());

    // 中央分组视图可能先于 client 注册创建：立即刷新一次完成挂载
    if (dock::PanelGroup* central_group = host_->centralGroup()) {
        if (PanelGroupItem* view = DockRuntime::instance().panelGroupItem(central_group))
            view->syncFromGroup();
    }
}

void DockHostItem::placePanel(QQuickItem* panel, DockEdge edge, QQuickItem* relative_to,
    QSize preferred_size, PanelLaunch launch)
{
    auto* instantiator = qobject_cast<DockPanelItem*>(panel);
    if (!host_ || !instantiator || !instantiator->panel())
        return;

    auto* relative_instantiator = qobject_cast<DockPanelItem*>(relative_to);
    dock::DockPanel* dock = instantiator->panel();

    host_->placePanel(dock, edge,
        relative_instantiator ? relative_instantiator->panel() : nullptr, preferred_size, launch);

    connect(dock, &dock::DockPanel::shownChanged, this, [this](bool) {
        if (area_item_)
            area_item_->sync();
    });
    connect(dock, &dock::DockPanel::detachedChanged, this, [this](bool) {
        if (area_item_)
            area_item_->sync();
    });

    if (area_item_)
        area_item_->sync();
}

void DockHostItem::stackPanel(QQuickItem* panel)
{
    auto* instantiator = qobject_cast<DockPanelItem*>(panel);
    if (!host_ || !instantiator || !instantiator->panel())
        return;

    host_->stackPanel(instantiator->panel());
    if (area_item_)
        area_item_->sync();
}

DockObject* DockHostItem::dockObject() const
{
    return host_;
}

void DockHostItem::applyFrame(const QRect& geometry)
{
    setPosition(geometry.topLeft());
    setSize(geometry.size());
}

QRect DockHostItem::frame() const
{
    return QRect(position().toPoint(), size().toSize());
}

void DockHostItem::applyVisibility(bool visible)
{
    setVisible(visible);
}

bool DockHostItem::isShown() const
{
    return isVisible();
}

QSize DockHostItem::minExtent() const
{
    return QSize(0, 0);
}

QSize DockHostItem::maxExtent() const
{
    return QSize(kMaxSizeLimit, kMaxSizeLimit);
}

void DockHostItem::bringToFront()
{
    setZ(z() + 1);
}

void DockHostItem::setCursorShape(Qt::CursorShape shape)
{
    setCursor(shape);
}

Qt::CursorShape DockHostItem::cursorShape() const
{
    return cursor().shape();
}

QPoint DockHostItem::globalOrigin() const
{
    return mapToGlobal(QPointF(0, 0)).toPoint();
}

DockView* DockHostItem::createDockWindow(DockObject* controller)
{
    auto* window = qobject_cast<dock::DockWindow*>(controller);
    if (!window)
        return nullptr;

    auto* view = new DockWindowItem(window);
    // 窗口在收到首个有效几何后自行显示（见 DockWindowItem::updateFromController）
    return view;
}

}
