/**
 * @file GuestView.h
 * @brief 由 QML 内容项直接构成的 guest 视图适配器（中央持久部件使用）
 */

#pragma once

#include "core/View.h"

QT_BEGIN_NAMESPACE
class QQuickItem;
QT_END_NAMESPACE

namespace dock {

class Controller;

namespace qtquick {

/**
 * @brief guest 视图适配器
 *
 * 把 QML 内容项包装成布局层的 View。尺寸约束取内容项的隐式尺寸；
 * 几何与可见性由 GroupView 直接管理，故其余接口为空实现。
 */
class GuestView : public View
{
public:
    GuestView(Controller* controller, QQuickItem* guest_item);

    Controller* controller() const override { return controller_; }
    void setViewGeometry(const QRect& geometry) override;
    QRect viewGeometry() const override;
    void setViewVisible(bool visible) override;
    bool isViewVisible() const override;
    QSize viewMinSize() const override;
    QSize viewMaxSizeHint() const override;
    void setParentView(View* parent) override { parent_view_ = parent; }
    View* parentView() const override { return parent_view_; }
    void raiseView() override;
    void setViewCursor(Qt::CursorShape shape) override;
    Qt::CursorShape viewCursor() const override;
    QPoint viewGlobalPosition() const override;
    View* createFloatingWindowView(Controller* controller) override;

private:
    Controller* controller_ = nullptr;
    QQuickItem* guest_item_ = nullptr;
    View* parent_view_ = nullptr;
};

}
}
