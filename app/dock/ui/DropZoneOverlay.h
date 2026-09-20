/**
 * @file DropZoneOverlay.h
 * @brief 透明顶层落点指示器窗口：始终显示在被拖动窗口之上
 */

#pragma once

#include "docking/DockEnums.h"

#include <QList>
#include <QRect>
#include <QQuickWindow>

class QQuickItem;

namespace dock::ui {

/**
 * @brief 指示器浮层窗口
 *
 * 拖拽悬停时覆盖"命中的停靠区域窗口"（主窗口或浮动窗口），在透明背景上
 * 同时绘制全部候选落点方框（分组内为上/下/左/右/合并 5 个），当前命中的
 * 一个加深高亮；因其为顶层窗口并在每次更新时 raise，不会被跟随光标的
 * 浮动窗口遮挡。窗口不接受鼠标与焦点。
 */
class DropZoneOverlay : public QQuickWindow
{
    Q_OBJECT
public:
    //! @brief 单个指示器：落点、矩形（全局坐标）、是否为当前命中项
    struct ZoneRectHit {
        DropZone location = DropZone::None;
        QRect rect;
        bool active = false;
    };

    explicit DropZoneOverlay(QObject* parent = nullptr);
    ~DropZoneOverlay() override;

    /**
     * @brief 显示指示器方框
     * @param zones 待显示的指示器列表（全局坐标）
     * @param area_global_rect 命中区域的全局矩形（浮层覆盖范围）
     * @param request_owner 请求方（用于避免其他区域误清除）
     * @param target_frame_global 目标分组描边（全局坐标，空矩形表示不描边）
     * @param tab_insert_global 标签插入标记（全局坐标，空矩形表示不显示）
     */
    void showZoneRects(const QList<ZoneRectHit>& zones,
        const QRect& area_global_rect, QQuickItem* request_owner,
        const QRect& target_frame_global = QRect(),
        const QRect& tab_insert_global = QRect());
    //! @brief 清除指示器（仅当请求方匹配时生效）
    void clear(QQuickItem* request_owner);

    //! @brief 当前是否有指示器
    bool isActive() const;
    //! @brief 当前指示器数量
    int zoneCount() const;
    //! @brief 当前标签插入标记（全局坐标；无标记时为空矩形）
    QRect tabInsertRect() const { return tab_insert_rect_; }
    //! @brief 当前请求方
    QQuickItem* requestOwner() const;

private:
    QQuickItem* root_item_ = nullptr;
    QQuickItem* request_owner_ = nullptr;
    QRect tab_insert_rect_;
};

}
