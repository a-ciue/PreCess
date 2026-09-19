/**
 * @file NodeMetrics.h
 * @brief 布局节点的几何与尺寸约束信息
 */

#pragma once

#include <QRect>
#include <QSize>
#include <Qt>

#include <algorithm>

namespace dock {

//! @brief 尺寸上限哨兵值（与 Qt 的 QWIDGETSIZE_MAX 相同），表示不限制大小
inline constexpr int kMaxSizeLimit = 16777215;

//! @brief 分隔条厚度（像素）
inline constexpr int kDividerThickness = 5;

/**
 * @brief 单个布局节点的几何与尺寸约束
 *
 * share 为该节点在父容器主轴上的长度占比（0.0~1.0），
 * 父容器尺寸变化时按占比重新分配主轴长度。
 */
struct NodeMetrics {
    //! @brief 布局几何（父容器坐标系）
    QRect geometry;
    //! @brief 硬最小尺寸
    QSize minExtent { 0, 0 };
    //! @brief 软最大尺寸提示
    QSize maxExtent { kMaxSizeLimit, kMaxSizeLimit };
    //! @brief 主轴长度占比
    double share = 0.0;

    //! @brief 主轴方向上的长度
    int length(Qt::Orientation orientation) const
    {
        return orientation == Qt::Horizontal ? geometry.width() : geometry.height();
    }

    //! @brief 主轴方向上的起点坐标
    int position(Qt::Orientation orientation) const
    {
        return orientation == Qt::Horizontal ? geometry.x() : geometry.y();
    }

    //! @brief 交叉轴方向上的长度
    int crossLength(Qt::Orientation orientation) const
    {
        return orientation == Qt::Horizontal ? geometry.height() : geometry.width();
    }

    //! @brief 将尺寸限制到 [minExtent, maxExtent]
    QSize boundedSize(const QSize& size) const
    {
        const int min_width = minExtent.width();
        const int min_height = minExtent.height();
        return QSize(std::clamp(size.width(), min_width, std::max(min_width, maxExtent.width())),
            std::clamp(size.height(), min_height, std::max(min_height, maxExtent.height())));
    }
};

}
