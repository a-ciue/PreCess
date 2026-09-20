/**
 * @file DropResolver.h
 * @brief 拖放落点解析：全局坐标 → 落点目标
 */

#pragma once

#include "DropTarget.h"

class QPoint;

namespace dock {

class DockRegion;

/**
 * @brief 拖放落点解析器
 *
 * 把"光标在何处、可落到哪里"的判定从拖拽会话中独立出来：
 * 浮动窗口后进先出优先、其次主区域；分组内先查内方框（中央持久
 * 分组的中心合并落点被抑制），未命中回退外方框；区域内的空隙按
 * 外方框判定。
 */
class DropResolver
{
public:
    /**
     * @brief 解析全局坐标处的落点目标
     * @param global_pos 全局屏幕坐标
     * @param dragged_region 拖拽源所在区域（解析时排除，可空）
     */
    static DropTarget resolve(const QPoint& global_pos, const DockRegion* dragged_region);
};

}
