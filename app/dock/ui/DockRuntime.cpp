/**
 * @file DockRuntime.cpp
 * @brief QtQuick 视图层全局服务的实现
 */

#include "DockRuntime.h"

#include "PanelGroupItem.h"
#include "DropZoneOverlay.h"
#include "docking/DockPanel.h"
#include "docking/PanelGroup.h"

#include <QCoreApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>

namespace dock::ui {

DockRuntime& DockRuntime::instance()
{
    static DockRuntime platform;
    return platform;
}

QQuickItem* DockRuntime::createItemFromUrl(const QUrl& url)
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

void DockRuntime::registerPanelContent(DockPanel* panel, QQuickItem* guest_item)
{
    if (panel && guest_item)
        panel_contents_.insert(panel, guest_item);
}

void DockRuntime::unregisterPanelContent(DockPanel* panel)
{
    panel_contents_.remove(panel);
}

QQuickItem* DockRuntime::panelContentItem(DockPanel* panel) const
{
    return panel_contents_.value(panel, nullptr);
}

void DockRuntime::registerPanelHome(DockPanel* panel, QQuickItem* home_item)
{
    if (panel && home_item)
        panel_homes_.insert(panel, home_item);
}

void DockRuntime::unregisterPanelHome(DockPanel* panel)
{
    panel_homes_.remove(panel);
}

QQuickItem* DockRuntime::panelHome(DockPanel* panel) const
{
    return panel_homes_.value(panel, nullptr);
}

PanelGroupItem* DockRuntime::panelGroupItem(PanelGroup* group)
{
    if (!group)
        return nullptr;

    PanelGroupItem* view = group_items_.value(group, nullptr);
    if (!view) {
        view = new PanelGroupItem(group);
        group_items_.insert(group, view);
        // 注册为分组视图：核心层经 DockView 查询标签插入位置等视图信息
        group->setView(view);
        QObject::connect(group, &QObject::destroyed, view, [this, group] {
            PanelGroupItem* dying = group_items_.take(group);
            if (dying)
                dying->deleteLater();
        });
        // 面板全部移走后的空分组不再需要视图
        QObject::connect(group, &PanelGroup::panelsChanged, view, [this, group] {
            if (group->panels().isEmpty())
                destroyPanelGroupItem(group);
        });
    }
    return view;
}

PanelGroupItem* DockRuntime::existingPanelGroupItem(PanelGroup* group) const
{
    return group ? group_items_.value(group, nullptr) : nullptr;
}

void DockRuntime::forgetPanelGroupItem(PanelGroup* group, PanelGroupItem* view)
{
    if (group_items_.value(group, nullptr) == view)
        group_items_.remove(group);
}

void DockRuntime::destroyPanelGroupItem(PanelGroup* group)
{
    PanelGroupItem* view = group_items_.take(group);
    if (!view)
        return;

    // 立即隐藏，避免等待 deleteLater 期间以旧几何遮挡相邻分组
    view->setVisible(false);
    view->deleteLater();
}

DropZoneOverlay* DockRuntime::zonesOverlay()
{
    if (!overlay_ && QCoreApplication::instance()) {
        // 进程内常驻单例：不设 QObject 父，避免退出期销毁 QQuickWindow 的时序问题
        overlay_ = new DropZoneOverlay();
    }
    return overlay_;
}

}
