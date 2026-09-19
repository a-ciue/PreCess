/**
 * @file PanelContentView.h
 * @brief 由 QML 内容项直接构成的 client 视图适配器（中央持久部件使用）
 */

#pragma once

#include "docking/DockView.h"

QT_BEGIN_NAMESPACE
class QQuickItem;
QT_END_NAMESPACE

namespace dock {

class DockObject;

namespace ui {

/**
 * @brief client 视图适配器
 *
 * 把 QML 内容项包装成布局层的 DockView。尺寸约束取内容项的隐式尺寸；
 * 几何与可见性由 PanelGroupItem 直接管理，故其余接口为空实现。
 */
class PanelContentView : public DockView
{
public:
    PanelContentView(DockObject* controller, QQuickItem* guest_item);

    DockObject* dockObject() const override { return dock_object_; }
    void applyFrame(const QRect& geometry) override;
    QRect frame() const override;
    void applyVisibility(bool visible) override;
    bool isShown() const override;
    QSize minExtent() const override;
    QSize maxExtent() const override;
    void setParentDockView(DockView* parent) override { parent_view_ = parent; }
    DockView* parentDockView() const override { return parent_view_; }
    void bringToFront() override;
    void setCursorShape(Qt::CursorShape shape) override;
    Qt::CursorShape cursorShape() const override;
    QPoint globalOrigin() const override;
    DockView* createDockWindow(DockObject* controller) override;

private:
    DockObject* dock_object_ = nullptr;
    QQuickItem* panel_content_item_ = nullptr;
    DockView* parent_view_ = nullptr;
};

}
}
