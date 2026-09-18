/**
 * @file SeparatorView.cpp
 * @brief 分隔条视图的实现
 */

#include "SeparatorView.h"

#include "AreaItem.h"
#include "Platform.h"
#include "core/Controller.h"
#include "engine/ItemBoxContainer.h"
#include "engine/Separator.h"

#include <QVariant>

namespace dock::qtquick {

SeparatorView::SeparatorView(QQuickItem* parent)
    : QQuickItem(parent)
{
    QQuickItem* root = Platform::instance().createItemFromUrl(
        QUrl(QStringLiteral("qrc:/precess/dock/DockSeparator.qml")));
    if (!root)
        return;

    root->setParentItem(this);
    root->setProperty("separatorView", QVariant::fromValue<QObject*>(this));
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

SeparatorView::~SeparatorView() = default;

void SeparatorView::setSeparator(Separator* separator, AreaItem* area)
{
    separator_ = separator;
    area_ = area;
    Q_EMIT separatorChanged();
}

bool SeparatorView::isHorizontal() const
{
    return separator_ && separator_->container()
        && separator_->container()->orientation() == Qt::Horizontal;
}

void SeparatorView::beginDrag(const QPointF& global_pos)
{
    Q_UNUSED(global_pos);
    dragging_ = true;
}

void SeparatorView::dragTo(const QPointF& global_pos)
{
    if (!dragging_ || !separator_ || !area_)
        return;

    if (separator_->move(mainAxisPosition(global_pos)))
        area_->sync();
}

void SeparatorView::endDrag()
{
    dragging_ = false;
}

int SeparatorView::mainAxisPosition(const QPointF& global_pos) const
{
    if (!area_ || !separator_)
        return 0;

    const QPointF local = area_->mapFromGlobal(global_pos);
    return separator_->container()->orientation() == Qt::Horizontal
        ? qRound(local.x())
        : qRound(local.y());
}

}
