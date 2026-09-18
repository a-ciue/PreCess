/**
 * @file MouseGrabber.h
 * @brief 全局鼠标事件过滤器：拖拽期间统一转发移动/释放事件
 */

#pragma once

#include <QObject>

namespace dock::qtquick {

/**
 * @brief 全局鼠标事件过滤器
 *
 * 按下由 QML 视图交给 DragController；一旦进入按下/拖拽状态，
 * 安装于 QGuiApplication 的本过滤器拦截所有移动与左键释放事件并转发，
 * 避免浮动窗口跟随过程中事件丢失或被其他控件消费。
 */
class MouseGrabber : public QObject
{
    Q_OBJECT
public:
    explicit MouseGrabber(QObject* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
};

}
