/**
 * @file Platform.h
 * @brief QtQuick 视图层的全局服务：QML 引擎、guest 映射、分组视图缓存
 */

#pragma once

#include <QHash>
#include <QUrl>

QT_BEGIN_NAMESPACE
class QQmlEngine;
class QQuickItem;
QT_END_NAMESPACE

namespace dock {

class DockWidget;
class Group;

namespace qtquick {

class GroupView;
class IndicatorsOverlayWindow;

/**
 * @brief QtQuick 视图层全局服务
 *
 * 核心层（QtCore）与视图层（QtQuick）之间的桥：保存 QML 引擎用于动态加载
 * 视图 QML、维护停靠面板到 guest item 的映射、按分组缓存分组视图。
 */
class Platform
{
public:
    //! @brief 单例
    static Platform& instance();

    //! @brief QML 引擎（由 dock::init 注入）
    QQmlEngine* engine() const { return engine_; }
    //! @brief 注入 QML 引擎
    void setEngine(QQmlEngine* engine) { engine_ = engine; }

    //! @brief 从 qrc/文件路径创建 QML 根项；失败返回 nullptr
    QQuickItem* createItemFromUrl(const QUrl& url);

    //! @brief 登记面板的 guest item（QML 声明的内容项）
    void registerGuestItem(DockWidget* dock_widget, QQuickItem* guest_item);
    //! @brief 注销面板的 guest item
    void unregisterGuestItem(DockWidget* dock_widget);
    //! @brief 面板的 guest item（可能为空）
    QQuickItem* guestItem(DockWidget* dock_widget) const;

    //! @brief 取得（必要时创建）分组视图；所有权由本服务持有
    GroupView* groupView(Group* group);
    //! @brief 从缓存中注销分组视图（视图析构或分组清空时）
    void forgetGroupView(Group* group, GroupView* view);
    //! @brief 删除分组视图（分组销毁时）
    void destroyGroupView(Group* group);

    //! @brief 透明顶层指示器窗口（懒创建，随应用存活）
    IndicatorsOverlayWindow* indicatorsOverlay();

private:
    Platform() = default;

    QQmlEngine* engine_ = nullptr;
    QHash<DockWidget*, QQuickItem*> guest_items_;
    QHash<Group*, GroupView*> group_views_;
    IndicatorsOverlayWindow* overlay_ = nullptr;
};

}
}
