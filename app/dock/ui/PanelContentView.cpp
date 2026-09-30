/**
 * @file PanelContentView.cpp
 * @brief client 视图适配器的实现
 */

#include "PanelContentView.h"

#include "tree/NodeMetrics.h"
#include "docking/DockMetrics.h"

#include <QQuickItem>
#include <QtMath>

namespace dock::ui {

PanelContentView::PanelContentView(DockObject* controller, QQuickItem* guest_item)
    : dock_object_(controller)
    , panel_content_item_(guest_item)
{
}

void PanelContentView::applyFrame(const QRect& geometry)
{
    if (panel_content_item_)
        panel_content_item_->setSize(geometry.size());
}

QRect PanelContentView::frame() const
{
    if (!panel_content_item_)
        return {};
    return QRect(panel_content_item_->position().toPoint(), panel_content_item_->size().toSize());
}

void PanelContentView::applyVisibility(bool visible)
{
    if (panel_content_item_)
        panel_content_item_->setVisible(visible);
}

bool PanelContentView::isShown() const
{
    return panel_content_item_ && panel_content_item_->isVisible();
}

QSize PanelContentView::minExtent() const
{
    if (!panel_content_item_)
        return QSize(0, 0);
    return QSize(qCeil(panel_content_item_->implicitWidth()), qCeil(panel_content_item_->implicitHeight()));
}

QSize PanelContentView::maxExtent() const
{
    return QSize(kMaxSizeLimit, kMaxSizeLimit);
}

void PanelContentView::bringToFront()
{
    if (panel_content_item_)
        // 层 z 受控（内容视图不参与无界自增）
        panel_content_item_->setZ(DockMetrics::kGroupLayerZ);
}

QPoint PanelContentView::globalOrigin() const
{
    return panel_content_item_ ? panel_content_item_->mapToGlobal(QPointF(0, 0)).toPoint() : QPoint(0, 0);
}

DockView* PanelContentView::createDockWindow(DockObject* controller)
{
    Q_UNUSED(controller);
    return nullptr;
}

}
