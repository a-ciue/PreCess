/**
 * @file IndicatorsOverlayWindow.h
 * @brief 透明顶层落点指示器窗口：始终显示在被拖动窗口之上
 */

#pragma once

#include <QRect>
#include <QQuickWindow>

class QQuickItem;

namespace dock::qtquick {

/**
 * @brief 指示器浮层窗口
 *
 * 拖拽悬停时覆盖"命中的停靠区域窗口"（主窗口或浮动窗口），在透明背景上
 * 绘制落点高亮；因其为顶层窗口并在每次更新时 raise，不会被跟随光标的
 * 浮动窗口遮挡。窗口不接受鼠标与焦点。
 */
class IndicatorsOverlayWindow : public QQuickWindow
{
    Q_OBJECT
public:
    explicit IndicatorsOverlayWindow(QObject* parent = nullptr);
    ~IndicatorsOverlayWindow() override;

    /**
     * @brief 显示高亮
     * @param global_rect 高亮矩形（全局屏幕坐标）
     * @param area_global_rect 命中区域的全局矩形（浮层覆盖范围）
     * @param request_owner 请求方（用于避免其他区域误清除）
     */
    void showHighlight(const QRect& global_rect, const QRect& area_global_rect,
        QQuickItem* request_owner);
    //! @brief 清除高亮（仅当请求方匹配时生效）
    void clear(QQuickItem* request_owner);

    //! @brief 当前是否有高亮
    bool isActive() const;
    //! @brief 当前请求方
    QQuickItem* requestOwner() const;

private:
    QQuickItem* root_item_ = nullptr;
    QQuickItem* request_owner_ = nullptr;
};

}
