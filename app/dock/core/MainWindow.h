/**
 * @file MainWindow.h
 * @brief 主停靠窗口：持有根停靠区域与中央持久部件
 */

#pragma once

#include "Controller.h"
#include "DockTypes.h"

#include <QString>

namespace dock {

class DockWidget;
class DropArea;
class Group;
class Item;
class View;

/**
 * @brief 主停靠窗口
 *
 * 一个应用当前仅使用一个 MainWindow（对应 QML 的 DockingArea）。
 * MainWindowOption_HasCentralWidget 时创建不可关闭/浮动/拖动的中央持久部件。
 */
class MainWindow : public Controller
{
    Q_OBJECT
public:
    MainWindow(const QString& unique_name, MainWindowOptions options, QObject* parent = nullptr);
    ~MainWindow() override;

    //! @brief 唯一名称
    QString uniqueName() const { return unique_name_; }
    //! @brief 选项
    MainWindowOptions options() const { return options_; }

    //! @brief 根停靠区域
    DropArea* dropArea() const { return drop_area_; }

    //! @brief 中央持久面板（无中央部件时为空）
    DockWidget* centralDockWidget() const { return central_dock_widget_; }
    //! @brief 中央分组（无中央部件时为空）
    Group* centralGroup() const { return central_group_; }
    //! @brief 中央布局节点（无中央部件时为空）
    Item* centralItem() const { return central_item_; }

    //! @brief 设置中央部件内容视图（由视图层加载 QML 后注入）
    void setCentralGuestView(View* guest_view);

    /**
     * @brief 命令式添加停靠面板
     * @sa DropArea::addDockWidget
     */
    void addDockWidget(DockWidget* dock_widget, Location location, DockWidget* relative_to,
        const QSize& preferred_size, InitialVisibilityOption option = StartVisible);

    //! @brief 以选项卡方式加入中央分组
    void addDockWidgetAsTab(DockWidget* dock_widget);

    //! @brief 应用窗口内容区几何（视图层尺寸变化时调用）
    void setAreaGeometry(const QRect& geometry);

Q_SIGNALS:
    //! @brief 布局尺寸变化
    void areaGeometryChanged(const QRect& geometry);

private:
    QString unique_name_;
    MainWindowOptions options_ = MainWindowOption_None;
    DropArea* drop_area_ = nullptr;
    DockWidget* central_dock_widget_ = nullptr;
    Group* central_group_ = nullptr;
    Item* central_item_ = nullptr;
};

}
