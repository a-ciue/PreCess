/**
 * @file DockRuntime.h
 * @brief QtQuick 视图层的全局服务：QML 引擎、client 映射、分组视图缓存
 */

#pragma once

#include <QHash>
#include <QUrl>

QT_BEGIN_NAMESPACE
class QQmlEngine;
class QQuickItem;
QT_END_NAMESPACE

namespace dock {

class DockPanel;
class PanelGroup;

namespace ui {

class PanelGroupItem;
class DropZoneOverlay;

/**
 * @brief QtQuick 视图层全局服务
 *
 * 核心层（QtCore）与视图层（QtQuick）之间的桥：保存 QML 引擎用于动态加载
 * 视图 QML、维护停靠面板到 client item 的映射、按分组缓存分组视图。
 */
class DockRuntime
{
public:
    //! @brief 单例
    static DockRuntime& instance();

    //! @brief QML 引擎（由 dock::init 注入）
    QQmlEngine* engine() const { return engine_; }
    //! @brief 注入 QML 引擎
    void setEngine(QQmlEngine* engine) { engine_ = engine; }

    //! @brief 从 qrc/文件路径创建 QML 根项；失败返回 nullptr
    QQuickItem* createItemFromUrl(const QUrl& url);

    //! @brief 登记面板的 client item（QML 声明的内容项）
    void registerPanelContent(DockPanel* panel, QQuickItem* guest_item);
    //! @brief 注销面板的 client item
    void unregisterPanelContent(DockPanel* panel);
    //! @brief 面板的 client item（可能为空）
    QQuickItem* panelContentItem(DockPanel* panel) const;

    //! @brief 取得（必要时创建）分组视图；所有权由本服务持有
    PanelGroupItem* panelGroupItem(PanelGroup* group);
    //! @brief 仅查询缓存中的分组视图（不创建），用于陈旧视图收口
    PanelGroupItem* existingPanelGroupItem(PanelGroup* group) const;
    //! @brief 从缓存中注销分组视图（视图析构或分组清空时）
    void forgetPanelGroupItem(PanelGroup* group, PanelGroupItem* view);
    //! @brief 删除分组视图（分组销毁时）
    void destroyPanelGroupItem(PanelGroup* group);

    //! @brief 透明顶层指示器窗口（懒创建，随应用存活）
    DropZoneOverlay* zonesOverlay();

private:
    DockRuntime() = default;

    QQmlEngine* engine_ = nullptr;
    QHash<DockPanel*, QQuickItem*> panel_contents_;
    QHash<PanelGroup*, PanelGroupItem*> group_items_;
    DropZoneOverlay* overlay_ = nullptr;
};

}
}
