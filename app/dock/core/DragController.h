/**
 * @file DragController.h
 * @brief 拖拽状态机：按下 → 超过阈值转拖拽 → 悬停落点 → 释放/取消
 */

#pragma once

#include "DockTypes.h"

#include <QObject>
#include <QPoint>
#include <QSize>

namespace dock {

class Draggable;
class DropArea;
class FloatingWindow;
class Group;
class WindowBeingDragged;

/**
 * @brief 拖拽状态机
 *
 * 视图层（全局鼠标过滤器 / QML 标题栏）在按下、移动、释放时驱动本控制器；
 * 拖拽开始时把分组摘入浮动窗口，释放时按悬停落点停靠或保留浮动。
 * 本控制器为核心层单例，不依赖 QtQuick。
 */
class DragController : public QObject
{
    Q_OBJECT
public:
    //! @brief 状态
    enum class State {
        Idle, //!< 空闲
        Pressed, //!< 已按下但未超过拖拽阈值
        Dragging //!< 拖拽中
    };
    Q_ENUM(State)

    //! @brief 单例
    static DragController& self();

    //! @brief 当前状态
    State state() const { return state_; }
    //! @brief 当前拖拽源（空闲时为空）
    Draggable* draggable() const { return draggable_; }
    //! @brief 当前被拖拽的浮动窗口（空闲时为空）
    FloatingWindow* draggedFloatingWindow() const;

    //! @brief 悬停命中的停靠区域
    DropArea* hoveredArea() const { return hovered_area_; }
    //! @brief 悬停命中的分组（外落点为空）
    Group* hoveredGroup() const { return hovered_group_; }
    //! @brief 当前悬停落点
    DropLocation hoveredLocation() const { return hovered_location_; }

    //! @brief 视图层在标题栏/选项卡按下时调用
    void onPress(Draggable* draggable, const QPoint& global_pos);
    //! @brief 视图层转发鼠标移动
    void onMove(const QPoint& global_pos);
    //! @brief 视图层转发鼠标释放
    void onRelease(const QPoint& global_pos);
    //! @brief 取消当前按下/拖拽（Esc、窗口失焦等）
    void cancel();

    //! @brief 使分组浮动（浮动按钮/双击）；成功返回 true
    bool floatGroup(Group* group);
    //! @brief 使浮动分组回停到主窗口占位处
    bool dockGroup(Group* group);
    //! @brief 切换分组浮动/停靠状态
    bool toggleFloating(Group* group);

Q_SIGNALS:
    //! @brief 状态变化
    void stateChanged(State state);
    //! @brief 悬停落点变化（视图层刷新指示器）
    void hoverChanged();
    //! @brief 命令式布局变化（浮动/回停），视图层需重新同步
    void layoutChanged();

private:
    explicit DragController(QObject* parent = nullptr);
    ~DragController() override;

    void startDrag(const QPoint& global_pos);
    void updateHover(const QPoint& global_pos);
    void applyDrop();
    void cleanup();

    //! @brief 创建浮动窗口核心对象并通知视图层创建窗口
    static FloatingWindow* createFloatingWindow();
    //! @brief 关闭并销毁浮动窗口核心对象
    static void destroyFloatingWindow(FloatingWindow* floating_window);
    //! @brief 查找承载指定分组的浮动窗口
    static FloatingWindow* floatingWindowForGroup(Group* group);

    State state_ = State::Idle;
    Draggable* draggable_ = nullptr;
    WindowBeingDragged* window_being_dragged_ = nullptr;
    DropArea* source_area_ = nullptr;
    DropArea* hovered_area_ = nullptr;
    Group* hovered_group_ = nullptr;
    DropLocation hovered_location_ = DropLocation_None;
    QPoint press_pos_;
    QSize dragged_size_;
};

}
