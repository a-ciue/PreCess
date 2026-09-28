/**
 * @file DockCatalog.h
 * @brief 停靠对象注册表（极简版）
 *
 * 仅维护主窗口、停靠面板与浮动窗口的登记，供布局与拖放内部查询；
 * 不提供按名称查找、多主窗口枚举与清空等高级接口。
 */

#pragma once

#include <QList>
#include <QPoint>

namespace dock {

class DockPanel;
class DockWindow;
class DockHost;
class DockRegion;
class PanelGroup;

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

    //! @brief 按屏幕坐标命中分组（浮动窗口优先，其次主区域）
    PanelGroup* groupAtGlobal(const QPoint& global_pos) const;

    /**
     * @brief 按鼠标悬停位置循环切换标签（键盘导航）
     *
     * 光标悬停的分组有多个显示面板时循环切换其标签；
     * 悬停分组无多标签（含未悬停到任何分组）时不切换。
     * @return 是否发生了切换
     */
    bool cyclePanelAt(const QPoint& global_pos, bool forward);

private:
    DockCatalog() = default;

    DockHost* host_ = nullptr;
    QList<DockPanel*> panels_;
    QList<DockWindow*> windows_;
};

}
