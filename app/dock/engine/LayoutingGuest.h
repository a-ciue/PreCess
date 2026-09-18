/**
 * @file LayoutingGuest.h
 * @brief 被布局引擎托管的实体接口
 */

#pragma once

#include <QRect>
#include <QSize>

namespace dock {

/**
 * @brief 被布局引擎托管的实体接口
 *
 * 由停靠语义层的视图（Group 等）实现，布局引擎仅通过该接口
 * 读写几何与可见性，不感知具体的 UI 类型。
 */
class LayoutingGuest
{
public:
    virtual ~LayoutingGuest() = default;

    //! @brief 由引擎调用，应用布局几何
    virtual void setGuestGeometry(const QRect& geometry) = 0;
    //! @brief 硬最小尺寸
    virtual QSize minSize() const = 0;
    //! @brief 软最大尺寸提示
    virtual QSize maxSizeHint() const = 0;
    //! @brief 由引擎调用，应用可见性
    virtual void setGuestVisible(bool visible) = 0;
};

}
