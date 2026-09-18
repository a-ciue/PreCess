/**
 * @file FloatingWindowView.h
 * @brief 浮动窗口视图：无边框顶层窗口 + 拖拽/边缘缩放
 */

#pragma once

#include "core/View.h"

#include <QQuickWindow>

namespace dock {

class FloatingWindow;

namespace qtquick {

class AreaItem;

/**
 * @brief 浮动窗口视图
 *
 * 无边框 Qt::Tool 顶层窗口；内容宿主由 DockFloatingWindow.qml 提供，
 * 其中嵌一个 AreaItem 支持在浮窗内继续停靠。窗口位置/尺寸由核心层
 * FloatingWindow::geometryChanged 驱动。
 */
class FloatingWindowView : public QQuickWindow, public View
{
    Q_OBJECT
public:
    explicit FloatingWindowView(FloatingWindow* floating_window);
    ~FloatingWindowView() override;

    // View
    Controller* controller() const override;
    void setViewGeometry(const QRect& geometry) override;
    QRect viewGeometry() const override;
    void setViewVisible(bool visible) override;
    bool isViewVisible() const override;
    QSize viewMinSize() const override;
    QSize viewMaxSizeHint() const override;
    void setParentView(View* parent) override { Q_UNUSED(parent); }
    View* parentView() const override { return nullptr; }
    void raiseView() override;
    void setViewCursor(Qt::CursorShape shape) override;
    Qt::CursorShape viewCursor() const override;
    QPoint viewGlobalPosition() const override;
    View* createFloatingWindowView(Controller* controller) override;

private:
    //! @brief 应用控制器的几何
    void updateFromController();
    //! @brief 同步内容宿主尺寸与区域几何
    void updateAreaHost();

    FloatingWindow* floating_window_ = nullptr;
    QQuickItem* root_item_ = nullptr;
    QQuickItem* area_host_ = nullptr;
    AreaItem* area_item_ = nullptr;
};

}
}
