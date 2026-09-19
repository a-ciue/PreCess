/**
 * @file DividerItem.cpp
 * @brief 分隔条视图的实现
 */

#include "DividerItem.h"

#include "DockAreaItem.h"
#include "DockRuntime.h"
#include "docking/DockObject.h"
#include "tree/BoxNode.h"
#include "tree/Divider.h"

#include <QVariant>

namespace dock::ui {

DividerItem::DividerItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    QQuickItem* root = DockRuntime::instance().createItemFromUrl(
        QUrl(QStringLiteral("qrc:/precess/dock/Divider.qml")));
    if (!root)
        return;

    root->setParentItem(this);
    root->setProperty("dividerItem", QVariant::fromValue<QObject*>(this));
    root->setSize(size());
    root->setVisible(isVisible());

    connect(this, &QQuickItem::widthChanged, this, [this, root] {
        root->setWidth(width());
    });
    connect(this, &QQuickItem::heightChanged, this, [this, root] {
        root->setHeight(height());
    });
    connect(this, &QQuickItem::visibleChanged, this, [this, root] {
        root->setVisible(isVisible());
    });
}

DividerItem::~DividerItem() = default;

void DividerItem::setDivider(Divider* separator, DockAreaItem* area)
{
    divider_ = separator;
    area_ = area;
    Q_EMIT separatorChanged();
}

bool DividerItem::isHorizontal() const
{
    return divider_ && divider_->container()
        && divider_->container()->orientation() == Qt::Horizontal;
}

void DividerItem::beginGroupDrag(const QPointF& global_pos)
{
    Q_UNUSED(global_pos);
    dragging_ = true;
}

void DividerItem::dragTo(const QPointF& global_pos)
{
    if (!dragging_ || !divider_ || !area_)
        return;

    if (divider_->move(mainAxisPosition(global_pos)))
        area_->sync();
}

void DividerItem::endDrag()
{
    dragging_ = false;
}

int DividerItem::mainAxisPosition(const QPointF& global_pos) const
{
    if (!area_ || !divider_)
        return 0;

    const QPointF local = area_->mapFromGlobal(global_pos);
    return divider_->container()->orientation() == Qt::Horizontal
        ? qRound(local.x())
        : qRound(local.y());
}

}
