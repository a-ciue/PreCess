/**
 * @file DragHandle.h
 * @brief 拖拽源：停靠分组的标题栏/选项卡或浮动窗口标题栏
 */

#pragma once

namespace dock {

class DockPanel;
class DockWindow;
class PanelGroup;
class DockView;

/**
 * @brief 拖拽源描述
 *
 * 由视图层在标题栏/标签按下时创建，DragSession 仅保存裸指针
 * （生命周期由视图层在本次按下-释放期间保证）。
 * panel() 非空表示拖动单个标签面板，否则拖动整个分组。
 */
class DragHandle
{
public:
    DragHandle(DockView* view, PanelGroup* group, DockWindow* window = nullptr,
        DockPanel* panel = nullptr);

    //! @brief 触发拖拽的视图（标题栏/标签）
    DockView* view() const { return view_; }
    //! @brief 被拖拽的分组
    PanelGroup* group() const { return group_; }
    //! @brief 拖动源为浮动窗口时非空
    DockWindow* window() const { return window_; }
    //! @brief 被拖拽的单个标签面板（为空表示整组拖拽）
    DockPanel* panel() const { return panel_; }
    //! @brief 是否源自浮动窗口
    bool isDetached() const { return window_ != nullptr; }

private:
    DockView* view_ = nullptr;
    PanelGroup* group_ = nullptr;
    DockWindow* window_ = nullptr;
    DockPanel* panel_ = nullptr;
};

}
