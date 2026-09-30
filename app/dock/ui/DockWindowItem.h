/**
 * @file DockWindowItem.h
 * @brief 浮动窗口视图：无边框顶层窗口 + 拖拽/边缘缩放
 */

#pragma once

#include "docking/DockView.h"

#include <QQuickWindow>

namespace dock {

class DockWindow;

namespace ui {

class DockAreaItem;

/**
 * @brief 浮动窗口视图
 *
 * 无边框 Qt::Tool 顶层窗口；内容宿主由 DockWindow.qml 提供，
 * 其中嵌一个 DockAreaItem 支持在浮窗内继续停靠。窗口位置/尺寸由核心层
 * DockWindow::geometryChanged 驱动。
 */
class DockWindowItem : public QQuickWindow, public DockView
{
    Q_OBJECT
public:
    explicit DockWindowItem(DockWindow* window);
    ~DockWindowItem() override;

    // DockView
    DockObject* dockObject() const override;
    void applyFrame(const QRect& geometry) override;
    QRect frame() const override;
    void applyVisibility(bool visible) override;
    bool isShown() const override;
    QSize minExtent() const override;
    QSize maxExtent() const override;
    void bringToFront() override;
    QPoint globalOrigin() const override;
    DockView* createDockWindow(DockObject* controller) override;

private:
    //! @brief 应用控制器的几何
    void updateFromController();
    //! @brief 同步内容宿主尺寸与区域几何
    void updateAreaHost();

    DockWindow* window_ = nullptr;
    QQuickItem* root_item_ = nullptr;
    QQuickItem* area_host_ = nullptr;
    DockAreaItem* area_item_ = nullptr;
};

}
}
