/**
 * @file AreaItem.h
 * @brief 布局区域视图：把 DropArea 的布局树同步为 GroupView/SeparatorView
 */

#pragma once

#include <QHash>
#include <QQuickItem>
#include <QSet>

namespace dock {

class DropArea;
class FloatingWindow;
class Group;
class Item;
class Separator;

namespace qtquick {

class SeparatorView;

/**
 * @brief 布局区域视图
 *
 * 主窗口与每个浮动窗口各持有一个。sync() 从 DropArea 布局树收集分组与
 * 分隔条，创建/复用/回收对应视图并同步几何；拖拽悬停时更新落点高亮。
 */
class AreaItem : public QQuickItem
{
    Q_OBJECT
public:
    explicit AreaItem(QQuickItem* parent = nullptr);
    ~AreaItem() override;

    //! @brief 绑定停靠区域
    void setDropArea(DropArea* drop_area);
    //! @brief 绑定的停靠区域
    DropArea* dropArea() const { return drop_area_; }

    //! @brief 本区域所属的浮动窗口（主窗口为空）
    void setFloatingWindow(FloatingWindow* floating_window) { floating_window_ = floating_window; }
    //! @brief 本区域所属的浮动窗口
    FloatingWindow* floatingWindow() const { return floating_window_; }

    //! @brief 将布局树同步为视图
    void sync();

private:
    //! @brief 递归收集分组
    void collectGroups(Item* item, QSet<Group*>& groups) const;
    //! @brief 递归收集分隔条
    void collectSeparators(Item* item, QSet<Separator*>& separators) const;
    //! @brief 根据拖拽悬停状态更新落点高亮
    void updateIndicators();
    //! @brief 销毁全部分隔条视图
    void clearSeparatorViews();

    DropArea* drop_area_ = nullptr;
    FloatingWindow* floating_window_ = nullptr;
    QHash<Separator*, SeparatorView*> separator_views_;
    bool syncing_ = false;
};

}
}
