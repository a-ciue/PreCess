/**
 * @file DockPanelItem.cpp
 * @brief QML 停靠面板实例化器的实现
 */

#include "DockPanelItem.h"

#include "DockRuntime.h"
#include "docking/DockPanel.h"
#include "tree/NodeMetrics.h"

#include <QQuickWindow>
#include <QtMath>

namespace dock::ui {

DockPanelItem::DockPanelItem(QQuickItem* parent)
    : QQuickItem(parent)
{
}

DockPanelItem::~DockPanelItem()
{
    DockRuntime::instance().unregisterPanelContent(panel_);
}

void DockPanelItem::setUniqueName(const QString& unique_name)
{
    if (unique_name_ == unique_name)
        return;
    unique_name_ = unique_name;
    Q_EMIT uniqueNameChanged();
}

QString DockPanelItem::title() const
{
    return panel_ ? panel_->title() : title_;
}

void DockPanelItem::setTitle(const QString& title)
{
    if (title_ == title && !panel_)
        return;

    title_ = title;
    if (panel_)
        panel_->setTitle(title);
    else
        Q_EMIT titleChanged();
}

bool DockPanelItem::isPanelShown() const
{
    return panel_ && panel_->isShown();
}

bool DockPanelItem::isDetached() const
{
    return panel_ && panel_->isDetached();
}

void DockPanelItem::setSource(const QString& source)
{
    if (source_ == source)
        return;
    source_ = source;
    Q_EMIT sourceChanged();
}

bool DockPanelItem::isClosable() const
{
    return panel_ ? panel_->hasFeature(dock::DockPanel::Feature::Closable) : closable_;
}

void DockPanelItem::setClosable(bool closable)
{
    if (closable_ == closable)
        return;
    closable_ = closable;
    if (panel_)
        panel_->setFeature(dock::DockPanel::Feature::Closable, closable);
    Q_EMIT featuresChanged();
}

bool DockPanelItem::isMovable() const
{
    return panel_ ? panel_->hasFeature(dock::DockPanel::Feature::Movable) : movable_;
}

void DockPanelItem::setMovable(bool movable)
{
    if (movable_ == movable)
        return;
    movable_ = movable;
    if (panel_)
        panel_->setFeature(dock::DockPanel::Feature::Movable, movable);
    Q_EMIT featuresChanged();
}

bool DockPanelItem::isFloatable() const
{
    return panel_ ? panel_->hasFeature(dock::DockPanel::Feature::Floatable) : floatable_;
}

void DockPanelItem::setFloatable(bool floatable)
{
    if (floatable_ == floatable)
        return;
    floatable_ = floatable;
    if (panel_)
        panel_->setFeature(dock::DockPanel::Feature::Floatable, floatable);
    Q_EMIT featuresChanged();
}

void DockPanelItem::showPanel()
{
    if (panel_)
        panel_->showPanel();
}

void DockPanelItem::hidePanel()
{
    if (panel_)
        panel_->hidePanel();
}

DockObject* DockPanelItem::dockObject() const
{
    return panel_;
}

void DockPanelItem::applyFrame(const QRect& geometry)
{
    Q_UNUSED(geometry);
}

QRect DockPanelItem::frame() const
{
    return {};
}

void DockPanelItem::applyVisibility(bool visible)
{
    Q_UNUSED(visible);
}

bool DockPanelItem::isShown() const
{
    return panel_content_item_ && panel_content_item_->isVisible();
}

QSize DockPanelItem::minExtent() const
{
    if (!panel_content_item_)
        return QSize(0, 0);
    return QSize(qCeil(panel_content_item_->implicitWidth()), qCeil(panel_content_item_->implicitHeight()));
}

QSize DockPanelItem::maxExtent() const
{
    return QSize(kMaxSizeLimit, kMaxSizeLimit);
}

void DockPanelItem::bringToFront()
{
}

void DockPanelItem::setCursorShape(Qt::CursorShape shape)
{
    Q_UNUSED(shape);
}

Qt::CursorShape DockPanelItem::cursorShape() const
{
    return Qt::ArrowCursor;
}

QPoint DockPanelItem::globalOrigin() const
{
    return mapToGlobal(QPointF(0, 0)).toPoint();
}

DockView* DockPanelItem::createDockWindow(DockObject* controller)
{
    Q_UNUSED(controller);
    return nullptr;
}

void DockPanelItem::componentComplete()
{
    QQuickItem::componentComplete();

    const QList<QQuickItem*> children = childItems();
    if (!children.isEmpty())
        panel_content_item_ = children.first();

    if (unique_name_.isEmpty())
        unique_name_ = objectName();
    if (unique_name_.isEmpty())
        unique_name_ = QStringLiteral("panel");

    panel_ = new dock::DockPanel(unique_name_, this);
    if (!title_.isEmpty())
        panel_->setTitle(title_);
    panel_->setContentView(this);

    // 应用 QML 声明的能力位（属性可能在 componentComplete 之前设置）
    panel_->setFeature(dock::DockPanel::Feature::Closable, closable_);
    panel_->setFeature(dock::DockPanel::Feature::Movable, movable_);
    panel_->setFeature(dock::DockPanel::Feature::Floatable, floatable_);

    DockRuntime::instance().registerPanelContent(panel_, panel_content_item_);

    connect(panel_, &dock::DockPanel::titleChanged, this, [this](const QString&) {
        Q_EMIT titleChanged();
    });
    connect(panel_, &dock::DockPanel::shownChanged, this, [this](bool) {
        Q_EMIT shownChanged();
    });
    connect(panel_, &dock::DockPanel::detachedChanged, this, [this](bool) {
        Q_EMIT detachedChanged();
    });

    // 实例化器本身不参与显示，内容由分组视图接管
    setVisible(false);
    setSize(QSizeF(0, 0));
}

}
