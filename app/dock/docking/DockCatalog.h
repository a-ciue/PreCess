/**
 * @file DockCatalog.h
 * @brief 停靠对象注册表（极简版）
 *
 * 仅维护主窗口、停靠面板与浮动窗口的登记，供布局与拖放内部查询；
 * 不提供按名称查找、多主窗口枚举与清空等高级接口。
 */

#pragma once

#include <QList>

namespace dock {

class DockPanel;
class DockWindow;
class DockHost;

class DockCatalog
{
public:
    //! @brief 单例
    static DockCatalog& self();

    //! @brief 设置唯一主窗口（空表示注销）
    void setHost(DockHost* host) { host_ = host; }
    //! @brief 唯一主窗口
    DockHost* host() const { return host_; }

    //! @brief 登记停靠面板
    void registerPanel(DockPanel* panel);
    //! @brief 注销停靠面板
    void unregisterPanel(DockPanel* panel);
    //! @brief 全部停靠面板
    const QList<DockPanel*>& panels() const { return panels_; }

    //! @brief 登记独立窗口
    void registerWindow(DockWindow* window);
    //! @brief 注销独立窗口
    void unregisterWindow(DockWindow* window);
    //! @brief 全部独立窗口
    const QList<DockWindow*>& windows() const { return windows_; }

    //! @brief 记录最近获得焦点的面板（Ctrl+Tab 切换用）
    void setFocusedPanel(DockPanel* panel) { focused_panel_ = panel; }
    //! @brief 最近获得焦点的面板（可能为空）
    DockPanel* focusedPanel() const { return focused_panel_; }

    /**
     * @brief 循环切换显示中的面板（键盘导航）
     *
     * 当前面板所在分组有多个显示面板时先循环切标签；
     * 否则按登记顺序切到下一个显示面板并提升其分组。
     * @return 是否发生了切换
     */
    bool cyclePanel(bool forward);

private:
    DockCatalog() = default;

    DockHost* host_ = nullptr;
    QList<DockPanel*> panels_;
    QList<DockWindow*> windows_;
    DockPanel* focused_panel_ = nullptr;
};

}
