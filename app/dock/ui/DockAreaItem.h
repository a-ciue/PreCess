/**
 * @file DockAreaItem.h
 * @brief 布局区域视图：把 DockRegion 的布局树同步为 PanelGroupItem/DividerItem
 */

#pragma once

#include <QHash>
#include <QQuickItem>
#include <QSet>

namespace dock {

class DockRegion;
class DockWindow;
class PanelGroup;
class LayoutNode;
class Divider;

namespace ui {

class DividerItem;

/**
 * @brief 布局区域视图
 *
 * 主窗口与每个浮动窗口各持有一个。sync() 从 DockRegion 布局树收集分组与
 * 分隔条，创建/复用/回收对应视图并同步几何；拖拽悬停时更新落点高亮。
 */
class DockAreaItem : public QQuickItem
{
    Q_OBJECT
public:
    explicit DockAreaItem(QQuickItem* parent = nullptr);
    ~DockAreaItem() override;

    //! @brief 绑定停靠区域
    void setRegion(DockRegion* region);
    //! @brief 绑定的停靠区域
    DockRegion* region() const { return region_; }

    //! @brief 本区域所属的浮动窗口（主窗口为空）
    void setOwningWindow(DockWindow* window) { window_ = window; }
    //! @brief 本区域所属的浮动窗口
    DockWindow* window() const { return window_; }

    //! @brief 将布局树同步为视图
    void sync();

private:
    //! @brief 递归收集分隔条
    void collectSeparators(LayoutNode* item, QSet<Divider*>& separators) const;
    //! @brief 根据拖拽悬停状态更新落点高亮
    void updateZoneRects();
    //! @brief 销毁全部分隔条视图
    void clearSeparatorViews();

    DockRegion* region_ = nullptr;
    DockWindow* window_ = nullptr;
    QHash<Divider*, DividerItem*> divider_views_;
    //! @brief 上一次同步归属本区域的分组（用于收口已离开的陈旧视图）
    QSet<PanelGroup*> synced_groups_;
    //! @brief 分组销毁监听连接（按分组去重，避免重复累积）
    QHash<PanelGroup*, QMetaObject::Connection> group_watches_;
    bool syncing_ = false;
};

}
}
