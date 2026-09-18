/**
 * @file DockRegistry.h
 * @brief 停靠对象注册表（极简版）
 *
 * 仅维护主窗口、停靠面板与浮动窗口的登记，供布局与拖放内部查询；
 * 不提供 KDDockWidgets 的 dockByName/mainDockingAreas/clear 等高级接口。
 */

#pragma once

#include <QList>

namespace dock {

class DockWidget;
class FloatingWindow;
class MainWindow;

class DockRegistry
{
public:
    //! @brief 单例
    static DockRegistry& self();

    //! @brief 设置唯一主窗口（空表示注销）
    void setMainWindow(MainWindow* main_window) { main_window_ = main_window; }
    //! @brief 唯一主窗口
    MainWindow* mainWindow() const { return main_window_; }

    //! @brief 登记停靠面板
    void registerDockWidget(DockWidget* dock_widget);
    //! @brief 注销停靠面板
    void unregisterDockWidget(DockWidget* dock_widget);
    //! @brief 全部停靠面板
    const QList<DockWidget*>& dockWidgets() const { return dock_widgets_; }

    //! @brief 登记浮动窗口
    void registerFloatingWindow(FloatingWindow* floating_window);
    //! @brief 注销浮动窗口
    void unregisterFloatingWindow(FloatingWindow* floating_window);
    //! @brief 全部浮动窗口
    const QList<FloatingWindow*>& floatingWindows() const { return floating_windows_; }

private:
    DockRegistry() = default;

    MainWindow* main_window_ = nullptr;
    QList<DockWidget*> dock_widgets_;
    QList<FloatingWindow*> floating_windows_;
};

}
