/**
 * @file IndicatorsOverlayWindow.cpp
 * @brief 透明顶层落点指示器窗口的实现
 */

#include "IndicatorsOverlayWindow.h"

#include "Platform.h"

#include <QQuickItem>

namespace dock::qtquick {

IndicatorsOverlayWindow::IndicatorsOverlayWindow(QObject* parent)
    : QQuickWindow()
{
    Q_UNUSED(parent);
    setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowTransparentForInput
        | Qt::WindowDoesNotAcceptFocus);
    setColor(Qt::transparent);

    root_item_ = Platform::instance().createItemFromUrl(
        QUrl(QStringLiteral("qrc:/precess/dock/DockIndicators.qml")));
    if (!root_item_)
        return;

    root_item_->setParentItem(contentItem());
    root_item_->setSize(contentItem()->size());
    root_item_->setVisible(true);

    connect(contentItem(), &QQuickItem::widthChanged, this, [this] {
        root_item_->setWidth(contentItem()->width());
    });
    connect(contentItem(), &QQuickItem::heightChanged, this, [this] {
        root_item_->setHeight(contentItem()->height());
    });
}

IndicatorsOverlayWindow::~IndicatorsOverlayWindow() = default;

void IndicatorsOverlayWindow::showHighlight(const QRect& global_rect,
    const QRect& area_global_rect, QQuickItem* request_owner)
{
    if (!root_item_)
        return;

    setGeometry(area_global_rect);
    const QRect local = global_rect.translated(-area_global_rect.topLeft());
    root_item_->setProperty("highlightX", local.x());
    root_item_->setProperty("highlightY", local.y());
    root_item_->setProperty("highlightWidth", local.width());
    root_item_->setProperty("highlightHeight", local.height());
    root_item_->setProperty("active", true);
    request_owner_ = request_owner;

    if (!isVisible())
        show();
    raise();
}

void IndicatorsOverlayWindow::clear(QQuickItem* request_owner)
{
    if (request_owner_ != request_owner)
        return;

    request_owner_ = nullptr;
    if (root_item_)
        root_item_->setProperty("active", false);
    hide();
}

bool IndicatorsOverlayWindow::isActive() const
{
    return request_owner_ != nullptr;
}

QQuickItem* IndicatorsOverlayWindow::requestOwner() const
{
    return request_owner_;
}

}
