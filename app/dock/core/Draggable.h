/**
 * @file Draggable.h
 * @brief 拖拽源：停靠分组的标题栏/选项卡或浮动窗口标题栏
 */

#pragma once

namespace dock {

class FloatingWindow;
class Group;
class View;

/**
 * @brief 拖拽源描述
 *
 * 由视图层在标题栏/选项卡按下时创建，DragController 仅保存裸指针
 * （生命周期由视图层在本次按下-释放期间保证）。
 */
class Draggable
{
public:
    Draggable(View* view, Group* group, FloatingWindow* floating_window = nullptr);

    //! @brief 触发拖拽的视图（标题栏/选项卡）
    View* view() const { return view_; }
    //! @brief 被拖拽的分组
    Group* group() const { return group_; }
    //! @brief 拖动源为浮动窗口时非空
    FloatingWindow* floatingWindow() const { return floating_window_; }
    //! @brief 是否源自浮动窗口
    bool isFloating() const { return floating_window_ != nullptr; }

private:
    View* view_ = nullptr;
    Group* group_ = nullptr;
    FloatingWindow* floating_window_ = nullptr;
};

}
