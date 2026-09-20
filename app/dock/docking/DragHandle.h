/**
 * @file DragHandle.h
 * @brief 拖拽源：停靠分组的标题栏/选项卡或浮动窗口标题栏
 */

#pragma once

#include <QPointer>

namespace dock {

class DockPanel;
class DockWindow;
class PanelGroup;
class DockView;

/**
 * @brief 拖拽源描述
 *
 * 由视图层在标题栏/标签按下时创建，DragSession 仅保存裸指针
 * （生命周期由视图层在本次按下-释放期间保证）；内部引用持弱指针，
 * 拖拽期间分组/面板被回收时自动失效，避免解引用悬空对象。
 * panel() 非空表示拖动单个标签面板，否则拖动整个分组。
 */
class DragHandle
{
public:
    DragHandle(DockView* view, PanelGroup* group, DockWindow* window = nullptr,
        DockPanel* panel = nullptr);

    //! @brief 触发拖拽的视图（标题栏/标签）
    DockView* view() const { return view_; }
    //! @brief 被拖拽的分组（已失效时为空）
    PanelGroup* group() const;
    //! @brief 拖动源为浮动窗口时非空
    DockWindow* window() const;
    //! @brief 被拖拽的单个标签面板（为空表示整组拖拽）
    DockPanel* panel() const;
    //! @brief 是否源自浮动窗口
    bool isDetached() const;

private:
    DockView* view_ = nullptr;
    QPointer<PanelGroup> group_;
    QPointer<DockWindow> window_;
    QPointer<DockPanel> panel_;
};

}
