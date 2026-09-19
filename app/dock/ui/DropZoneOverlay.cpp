/**
 * @file DropZoneOverlay.cpp
 * @brief 透明顶层落点指示器窗口的实现
 */

#include "DropZoneOverlay.h"

#include "DockRuntime.h"

#include <QQuickItem>
#include <QVariantList>
#include <QVariantMap>

namespace dock::ui {

DropZoneOverlay::DropZoneOverlay(QObject* parent)
    : QQuickWindow()
{
    Q_UNUSED(parent);
    setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowTransparentForInput
        | Qt::WindowDoesNotAcceptFocus);
    setColor(Qt::transparent);

    root_item_ = DockRuntime::instance().createItemFromUrl(
        QUrl(QStringLiteral("qrc:/precess/dock/DropZones.qml")));
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

DropZoneOverlay::~DropZoneOverlay() = default;

void DropZoneOverlay::showZoneRects(const QList<ZoneRectHit>& zones,
    const QRect& area_global_rect, QQuickItem* request_owner,
    const QRect& target_frame_global)
{
    if (!root_item_ || zones.isEmpty())
        return;

    setGeometry(area_global_rect);

    QVariantList model;
    model.reserve(zones.size());
    for (const ZoneRectHit& hit : zones) {
        const QRect local = hit.rect.translated(-area_global_rect.topLeft());
        QVariantMap entry;
        entry.insert(QStringLiteral("x"), local.x());
        entry.insert(QStringLiteral("y"), local.y());
        entry.insert(QStringLiteral("width"), local.width());
        entry.insert(QStringLiteral("height"), local.height());
        entry.insert(QStringLiteral("active"), hit.active);
        model.append(entry);
    }

    // 目标分组描边（浮层局部坐标）
    QVariantMap target;
    if (!target_frame_global.isEmpty()) {
        const QRect local = target_frame_global.translated(-area_global_rect.topLeft());
        target.insert(QStringLiteral("x"), local.x());
        target.insert(QStringLiteral("y"), local.y());
        target.insert(QStringLiteral("width"), local.width());
        target.insert(QStringLiteral("height"), local.height());
    }

    root_item_->setProperty("zones", model);
    root_item_->setProperty("targetFrame", target);
    request_owner_ = request_owner;

    if (!isVisible())
        show();
    raise();
}

void DropZoneOverlay::clear(QQuickItem* request_owner)
{
    if (request_owner_ != request_owner)
        return;

    request_owner_ = nullptr;
    if (root_item_) {
        root_item_->setProperty("zones", QVariantList());
        root_item_->setProperty("targetFrame", QVariantMap());
    }
    hide();
}

bool DropZoneOverlay::isActive() const
{
    return request_owner_ != nullptr;
}

int DropZoneOverlay::zoneCount() const
{
    return root_item_ ? root_item_->property("zones").toList().size() : 0;
}

QQuickItem* DropZoneOverlay::requestOwner() const
{
    return request_owner_;
}

}
