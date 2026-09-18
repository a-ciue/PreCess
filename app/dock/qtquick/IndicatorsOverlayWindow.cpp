/**
 * @file IndicatorsOverlayWindow.cpp
 * @brief 透明顶层落点指示器窗口的实现
 */

#include "IndicatorsOverlayWindow.h"

#include "Platform.h"

#include <QQuickItem>
#include <QVariantList>
#include <QVariantMap>

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

void IndicatorsOverlayWindow::showIndicators(const QList<IndicatorHit>& indicators,
    const QRect& area_global_rect, QQuickItem* request_owner)
{
    if (!root_item_ || indicators.isEmpty())
        return;

    setGeometry(area_global_rect);

    QVariantList model;
    model.reserve(indicators.size());
    for (const IndicatorHit& hit : indicators) {
        const QRect local = hit.rect.translated(-area_global_rect.topLeft());
        QVariantMap entry;
        entry.insert(QStringLiteral("x"), local.x());
        entry.insert(QStringLiteral("y"), local.y());
        entry.insert(QStringLiteral("width"), local.width());
        entry.insert(QStringLiteral("height"), local.height());
        entry.insert(QStringLiteral("active"), hit.active);
        model.append(entry);
    }

    root_item_->setProperty("indicators", model);
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
        root_item_->setProperty("indicators", QVariantList());
    hide();
}

bool IndicatorsOverlayWindow::isActive() const
{
    return request_owner_ != nullptr;
}

int IndicatorsOverlayWindow::indicatorCount() const
{
    return root_item_ ? root_item_->property("indicators").toList().size() : 0;
}

QQuickItem* IndicatorsOverlayWindow::requestOwner() const
{
    return request_owner_;
}

}
