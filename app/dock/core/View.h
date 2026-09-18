/**
 * @file View.h
 * @brief 停靠组件与视图层（QtQuick）之间的抽象接口
 */

#pragma once

#include <QRect>
#include <QSize>
#include <Qt>

namespace dock {

class Controller;

/**
 * @brief 视图抽象接口
 *
 * 核心层（控制器）通过该接口驱动界面的几何、可见性与光标，
 * 不依赖任何 QtQuick 类型。视图层（QQuickItem/QQuickWindow）实现该接口。
 * 视图对象由视图层（QML 引擎）持有，核心层只保存裸指针。
 */
class View
{
public:
    virtual ~View() = default;

    //! @brief 关联的控制器
    virtual Controller* controller() const = 0;

    //! @brief 应用几何（父视图坐标系）
    virtual void setViewGeometry(const QRect& geometry) = 0;
    //! @brief 当前几何
    virtual QRect viewGeometry() const = 0;
    //! @brief 应用可见性
    virtual void setViewVisible(bool visible) = 0;
    //! @brief 当前可见性
    virtual bool isViewVisible() const = 0;

    //! @brief 视图最小尺寸
    virtual QSize viewMinSize() const = 0;
    //! @brief 视图最大尺寸提示
    virtual QSize viewMaxSizeHint() const = 0;

    //! @brief 设置父视图（用于拖放时把分组视图移到浮动窗口下）
    virtual void setParentView(View* parent) = 0;
    //! @brief 父视图
    virtual View* parentView() const = 0;

    //! @brief 激活/置顶视图
    virtual void raiseView() = 0;
    //! @brief 设置鼠标光标形状
    virtual void setViewCursor(Qt::CursorShape shape) = 0;
    //! @brief 当前鼠标光标形状
    virtual Qt::CursorShape viewCursor() const = 0;

    //! @brief 视图全局原点（用于拖放命中测试）
    virtual QPoint viewGlobalPosition() const = 0;

    /**
     * @brief 创建浮动窗口视图
     *
     * 仅能创建顶层窗口的视图（主窗口视图）需要实现；
     * controller 为 FloatingWindow 控制器，返回其视图对象。
     */
    virtual View* createFloatingWindowView(Controller* controller) = 0;
};

}
