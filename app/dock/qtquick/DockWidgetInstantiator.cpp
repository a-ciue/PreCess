/**
 * @file DockWidgetInstantiator.cpp
 * @brief QML 停靠面板实例化器的实现
 */

#include "DockWidgetInstantiator.h"

#include "Platform.h"
#include "core/DockWidget.h"
#include "engine/SizingInfo.h"

#include <QQuickWindow>
#include <QtMath>

namespace dock::qtquick {

DockWidgetInstantiator::DockWidgetInstantiator(QQuickItem* parent)
    : QQuickItem(parent)
{
}

DockWidgetInstantiator::~DockWidgetInstantiator()
{
    Platform::instance().unregisterGuestItem(dock_widget_);
}

void DockWidgetInstantiator::setUniqueName(const QString& unique_name)
{
    if (unique_name_ == unique_name)
        return;
    unique_name_ = unique_name;
    Q_EMIT uniqueNameChanged();
}

QString DockWidgetInstantiator::title() const
{
    return dock_widget_ ? dock_widget_->title() : title_;
}

void DockWidgetInstantiator::setTitle(const QString& title)
{
    if (title_ == title && !dock_widget_)
        return;

    title_ = title;
    if (dock_widget_)
        dock_widget_->setTitle(title);
    else
        Q_EMIT titleChanged();
}

bool DockWidgetInstantiator::isOpen() const
{
    return dock_widget_ && dock_widget_->isOpen();
}

bool DockWidgetInstantiator::isFloating() const
{
    return dock_widget_ && dock_widget_->isFloating();
}

void DockWidgetInstantiator::setSource(const QString& source)
{
    if (source_ == source)
        return;
    source_ = source;
    Q_EMIT sourceChanged();
}

void DockWidgetInstantiator::open()
{
    if (dock_widget_)
        dock_widget_->open();
}

bool DockWidgetInstantiator::close()
{
    if (!dock_widget_)
        return false;
    dock_widget_->close();
    return true;
}

void DockWidgetInstantiator::show()
{
    open();
}

Controller* DockWidgetInstantiator::controller() const
{
    return dock_widget_;
}

void DockWidgetInstantiator::setViewGeometry(const QRect& geometry)
{
    Q_UNUSED(geometry);
}

QRect DockWidgetInstantiator::viewGeometry() const
{
    return {};
}

void DockWidgetInstantiator::setViewVisible(bool visible)
{
    Q_UNUSED(visible);
}

bool DockWidgetInstantiator::isViewVisible() const
{
    return guest_item_ && guest_item_->isVisible();
}

QSize DockWidgetInstantiator::viewMinSize() const
{
    if (!guest_item_)
        return QSize(0, 0);
    return QSize(qCeil(guest_item_->implicitWidth()), qCeil(guest_item_->implicitHeight()));
}

QSize DockWidgetInstantiator::viewMaxSizeHint() const
{
    return QSize(kMaxSizeLimit, kMaxSizeLimit);
}

void DockWidgetInstantiator::raiseView()
{
}

void DockWidgetInstantiator::setViewCursor(Qt::CursorShape shape)
{
    Q_UNUSED(shape);
}

Qt::CursorShape DockWidgetInstantiator::viewCursor() const
{
    return Qt::ArrowCursor;
}

QPoint DockWidgetInstantiator::viewGlobalPosition() const
{
    return mapToGlobal(QPointF(0, 0)).toPoint();
}

View* DockWidgetInstantiator::createFloatingWindowView(Controller* controller)
{
    Q_UNUSED(controller);
    return nullptr;
}

void DockWidgetInstantiator::componentComplete()
{
    QQuickItem::componentComplete();

    const QList<QQuickItem*> children = childItems();
    if (!children.isEmpty())
        guest_item_ = children.first();

    if (unique_name_.isEmpty())
        unique_name_ = objectName();
    if (unique_name_.isEmpty())
        unique_name_ = QStringLiteral("dockWidget");

    dock_widget_ = new dock::DockWidget(unique_name_, this);
    if (!title_.isEmpty())
        dock_widget_->setTitle(title_);
    dock_widget_->setGuestView(this);

    Platform::instance().registerGuestItem(dock_widget_, guest_item_);

    connect(dock_widget_, &dock::DockWidget::titleChanged, this, [this](const QString&) {
        Q_EMIT titleChanged();
    });
    connect(dock_widget_, &dock::DockWidget::isOpenChanged, this, [this](bool) {
        Q_EMIT isOpenChanged();
    });
    connect(dock_widget_, &dock::DockWidget::isFloatingChanged, this, [this](bool) {
        Q_EMIT isFloatingChanged();
    });

    // 实例化器本身不参与显示，内容由分组视图接管
    setVisible(false);
    setSize(QSizeF(0, 0));
}

}
