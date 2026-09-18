/**
 * @file IndicatorsOverlayWindow.h
 * @brief 透明顶层落点指示器窗口：始终显示在被拖动窗口之上
 */

#pragma once

#include "core/DockTypes.h"

#include <QList>
#include <QRect>
#include <QQuickWindow>

class QQuickItem;

namespace dock::qtquick {

/**
 * @brief 指示器浮层窗口
 *
 * 拖拽悬停时覆盖"命中的停靠区域窗口"（主窗口或浮动窗口），在透明背景上
 * 同时绘制全部候选落点方框（分组内为上/下/左/右/合并 5 个），当前命中的
 * 一个加深高亮；因其为顶层窗口并在每次更新时 raise，不会被跟随光标的
 * 浮动窗口遮挡。窗口不接受鼠标与焦点。
 */
class IndicatorsOverlayWindow : public QQuickWindow
{
    Q_OBJECT
public:
    //! @brief 单个指示器：落点、矩形（全局坐标）、是否为当前命中项
    struct IndicatorHit {
        DropLocation location = DropLocation_None;
        QRect rect;
        bool active = false;
    };

    explicit IndicatorsOverlayWindow(QObject* parent = nullptr);
    ~IndicatorsOverlayWindow() override;

    /**
     * @brief 显示指示器方框
     * @param indicators 待显示的指示器列表（全局坐标）
     * @param area_global_rect 命中区域的全局矩形（浮层覆盖范围）
     * @param request_owner 请求方（用于避免其他区域误清除）
     */
    void showIndicators(const QList<IndicatorHit>& indicators,
        const QRect& area_global_rect, QQuickItem* request_owner);
    //! @brief 清除指示器（仅当请求方匹配时生效）
    void clear(QQuickItem* request_owner);

    //! @brief 当前是否有指示器
    bool isActive() const;
    //! @brief 当前指示器数量
    int indicatorCount() const;
    //! @brief 当前请求方
    QQuickItem* requestOwner() const;

private:
    QQuickItem* root_item_ = nullptr;
    QQuickItem* request_owner_ = nullptr;
};

}
