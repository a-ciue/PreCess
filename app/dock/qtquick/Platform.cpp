/**
 * @file Platform.cpp
 * @brief QtQuick 视图层全局服务的实现
 */

#include "Platform.h"

#include "GroupView.h"
#include "core/DockWidget.h"
#include "core/Group.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>

namespace dock::qtquick {

Platform& Platform::instance()
{
    static Platform platform;
    return platform;
}

QQuickItem* Platform::createItemFromUrl(const QUrl& url)
{
    if (!engine_) {
        qWarning("dock: QML engine not set, call dock::init(engine) first");
        return nullptr;
    }

    QQmlComponent component(engine_, url);
    if (component.isError()) {
        qWarning("dock: failed to load %s: %s", qPrintable(url.toString()),
            qPrintable(component.errorString()));
        return nullptr;
    }

    QObject* object = component.create();
    auto* item = qobject_cast<QQuickItem*>(object);
    if (!item) {
        delete object;
        qWarning("dock: %s root is not a QQuickItem", qPrintable(url.toString()));
        return nullptr;
    }

    // 根项默认不可见，由布局同步决定
    item->setVisible(false);
    return item;
}

void Platform::registerGuestItem(DockWidget* dock_widget, QQuickItem* guest_item)
{
    if (dock_widget && guest_item)
        guest_items_.insert(dock_widget, guest_item);
}

void Platform::unregisterGuestItem(DockWidget* dock_widget)
{
    guest_items_.remove(dock_widget);
}

QQuickItem* Platform::guestItem(DockWidget* dock_widget) const
{
    return guest_items_.value(dock_widget, nullptr);
}

GroupView* Platform::groupView(Group* group)
{
    if (!group)
        return nullptr;

    GroupView* view = group_views_.value(group, nullptr);
    if (!view) {
        view = new GroupView(group);
        group_views_.insert(group, view);
        QObject::connect(group, &QObject::destroyed, view, [this, group] {
            GroupView* dying = group_views_.take(group);
            if (dying)
                dying->deleteLater();
        });
        // 面板全部移走后的空分组不再需要视图
        QObject::connect(group, &Group::dockWidgetsChanged, view, [this, group] {
            if (group->dockWidgets().isEmpty())
                destroyGroupView(group);
        });
    }
    return view;
}

void Platform::forgetGroupView(Group* group, GroupView* view)
{
    if (group_views_.value(group, nullptr) == view)
        group_views_.remove(group);
}

void Platform::destroyGroupView(Group* group)
{
    GroupView* view = group_views_.take(group);
    if (view)
        view->deleteLater();
}

}
