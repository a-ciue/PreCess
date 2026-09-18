/**
 * @file IndicatorsView.cpp
 * @brief 拖放落点高亮视图的实现
 */

#include "IndicatorsView.h"

#include "Platform.h"

#include <QVariant>

namespace dock::qtquick {

IndicatorsView::IndicatorsView(QQuickItem* parent)
    : QQuickItem(parent)
{
    QQuickItem* root = Platform::instance().createItemFromUrl(
        QUrl(QStringLiteral("qrc:/precess/dock/DockIndicators.qml")));
    if (!root)
        return;

    root->setParentItem(this);
    root->setProperty("indicatorView", QVariant::fromValue<QObject*>(this));
    root->setSize(size());
    root->setVisible(true);

    connect(this, &QQuickItem::widthChanged, this, [this, root] { root->setWidth(width()); });
    connect(this, &QQuickItem::heightChanged, this, [this, root] { root->setHeight(height()); });
}

IndicatorsView::~IndicatorsView() = default;

void IndicatorsView::setHighlight(const QRect& rect)
{
    if (highlight_ == rect)
        return;

    highlight_ = rect;
    Q_EMIT indicatorsChanged();
}

void IndicatorsView::clearHighlight()
{
    setHighlight(QRect());
}

}
