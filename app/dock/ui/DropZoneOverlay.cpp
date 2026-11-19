/**
 * @file DropZoneOverlay.cpp
 * @brief 透明顶层落点指示器窗口的实现
 */

#include "DropZoneOverlay.h"

#include "DockRuntime.h"

#include <QJSValue>
#include <QQuickItem>
#include <QRectF>
#include <QVariantList>
#include <QVariantMap>

namespace dock::ui {

DropZoneOverlay::DropZoneOverlay()
    : QQuickWindow()
{
    setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowTransparentForInput
        | Qt::WindowDoesNotAcceptFocus);
    setColor(Qt::transparent);

    root_item_ = DockRuntime::instance().createItemFromUrl(
        QUrl(QStringLiteral("qrc:/precess/dock/DropZones.qml")));
    if (!root_item_)
        return;

    root_item_->setParentItem(contentItem());
    // QQuickItem::setParentItem 不改 QObject 父子关系：显式接管所有权
    root_item_->setParent(this);
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
    const QRect& target_frame_global, const QRect& tab_insert_global)
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
    const QRectF target = target_frame_global.isEmpty()
        ? QRectF()
        : QRectF(target_frame_global.translated(-area_global_rect.topLeft()));

    // 标签插入标记（浮层局部坐标）
    tab_insert_rect_ = tab_insert_global;
    const QRectF tab_insert = tab_insert_global.isEmpty()
        ? QRectF()
        : QRectF(tab_insert_global.translated(-area_global_rect.topLeft()));

    root_item_->setProperty("zones", model);
    root_item_->setProperty("targetFrame", target);
    root_item_->setProperty("tabInsert", tab_insert);
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
    tab_insert_rect_ = QRect();
    if (root_item_) {
        root_item_->setProperty("zones", QVariantList());
        root_item_->setProperty("targetFrame", QRectF());
        root_item_->setProperty("tabInsert", QRectF());
    }
    hide();
}

bool DropZoneOverlay::isActive() const
{
    return request_owner_ != nullptr;
}

int DropZoneOverlay::zoneCount() const
{
    if (!root_item_)
        return 0;

    // QML var 列表可能返回 QJSValue，须先转换为原生 QVariantList。
    const QVariant zones = root_item_->property("zones");
    if (zones.metaType() == QMetaType::fromType<QJSValue>())
        return zones.value<QJSValue>().toVariant().toList().size();
    return zones.toList().size();
}

}
