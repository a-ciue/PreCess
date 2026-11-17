/**
 * @file GlobalMouseRelay.h
 * @brief 全局输入事件过滤器：拖拽鼠标事件转发 + Esc 取消 + Ctrl+Tab 标签切换
 */

#pragma once

#include <QObject>
#include <QPointF>

class QKeyEvent;

namespace dock::ui {

/**
 * @brief 全局输入事件过滤器
 *
 * 按下由 QML 视图交给 DragSession；一旦进入按下/拖拽状态，
 * 安装于 QGuiApplication 的本过滤器拦截所有移动与左键释放事件并转发，
 * 避免浮动窗口跟随过程中事件丢失或被其他控件消费。
 * 另提供两项会话级快捷键：拖拽中 Esc 取消并回弹、Ctrl+Tab 切换鼠标悬停分组的标签。
 */
class GlobalMouseRelay : public QObject
{
    Q_OBJECT
public:
    explicit GlobalMouseRelay(QObject* parent = nullptr);
    //! @brief 进程级单例；同时作为 QML 单例与事件过滤器使用。
    static GlobalMouseRelay* instance();

signals:
    //! @brief 全局左键按下（不拦截原事件），供应用层判断点击是否落在某个控件外。
    void leftPressed(const QPointF& global_pos);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    //! @brief 处理 Ctrl+Tab / Ctrl+Shift+Tab：切换悬停分组的标签
    static bool cyclePanels(bool forward);
};

}
