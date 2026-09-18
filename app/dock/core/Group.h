/**
 * @file Group.h
 * @brief 选项卡分组：承载一个或多个停靠面板的布局单元
 */

#pragma once

#include "Controller.h"
#include "engine/LayoutingGuest.h"

#include <QList>
#include <QSize>

namespace dock {

class DockWidget;
class Item;

/**
 * @brief 选项卡分组
 *
 * 分组是布局树的叶子实体（LayoutingGuest）：可见性由组内是否有打开的
 * 面板决定；关闭全部面板时分组整体让出布局空间但保留其在树中的位置，
 * 面板重新打开时恢复原位置与占比。
 */
class Group : public Controller, public LayoutingGuest
{
    Q_OBJECT
public:
    explicit Group(QObject* parent = nullptr);
    ~Group() override;

    //! @brief 加入面板（不改变开关状态）
    void addDockWidget(DockWidget* dock_widget);
    //! @brief 移除面板（当前仅用于对象销毁前的解绑）
    void removeDockWidget(DockWidget* dock_widget);

    //! @brief 组内全部面板（含已关闭）
    const QList<DockWidget*>& dockWidgets() const { return dock_widgets_; }
    //! @brief 组内打开的面板
    QList<DockWidget*> openDockWidgets() const;

    //! @brief 当前选项卡面板（可能为空）
    DockWidget* currentDockWidget() const { return current_; }
    //! @brief 设为当前选项卡（面板须在组内且已打开）
    void setCurrentDockWidget(DockWidget* dock_widget);
    //! @brief 当前选项卡在打开面板中的下标
    int currentIndex() const;
    //! @brief 按打开面板下标切换
    void setCurrentIndex(int index);

    //! @brief 分组标题（当前面板标题）
    QString title() const;

    //! @brief 是否为中央持久分组
    bool isCentral() const { return central_; }
    //! @brief 标记为中央持久分组
    void setIsCentral(bool central) { central_ = central; }

    //! @brief 布局树中的节点
    Item* layoutItem() const { return layout_item_; }
    //! @brief 设置布局节点（由布局层创建后注入）
    void setLayoutItem(Item* item) { layout_item_ = item; }

    //! @brief 根据打开面板情况同步分组可见性
    void refreshVisibility();

    // LayoutingGuest
    void setGuestGeometry(const QRect& geometry) override;
    QSize minSize() const override;
    QSize maxSizeHint() const override;
    void setGuestVisible(bool visible) override;

Q_SIGNALS:
    //! @brief 面板集合或开关状态变化
    void dockWidgetsChanged();
    //! @brief 当前选项卡变化
    void currentDockWidgetChanged(DockWidget* dock_widget);
    //! @brief 分组标题变化
    void titleChanged(const QString& title);

private:
    //! @brief 关闭的面板不再作为当前项时选择下一个可打开的面板
    void updateCurrentDockWidget();

    QList<DockWidget*> dock_widgets_;
    DockWidget* current_ = nullptr;
    Item* layout_item_ = nullptr;
    bool central_ = false;
};

}
