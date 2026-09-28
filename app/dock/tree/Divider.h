/**
 * @file Divider.h
 * @brief 相邻可见布局项之间的可拖动分隔条
 */

#pragma once

#include <QRect>

namespace dock {

class LayoutNode;
class BoxNode;

/**
 * @brief 分隔条
 *
 * 由 BoxNode::rebuildDividers() 在相邻可见子节点之间创建，
 * 拖动时在两侧子节点最小尺寸约束内移动，并触发容器按占比重排。
 */
class Divider
{
public:
    Divider(BoxNode* container, LayoutNode* side1, LayoutNode* side2);

    //! @brief 所属容器
    BoxNode* container() const { return container_; }
    //! @brief 前一个可见子节点
    LayoutNode* side1() const { return side1_; }
    //! @brief 后一个可见子节点
    LayoutNode* side2() const { return side2_; }

    //! @brief 沿容器主轴的位置（分隔条起始坐标）
    int position() const;
    //! @brief 可拖动的最小位置（侧 1 不小于其最小尺寸）
    int minPosition() const;
    //! @brief 可拖动的最大位置（侧 2 不小于其最小尺寸）
    int maxPosition() const;

    //! @brief 拖动到指定坐标；返回是否发生实际移动
    bool move(int newPosition);

    //! @brief 分隔条几何（由容器 layout() 更新）
    const QRect& geometry() const { return geometry_; }
    //! @brief 更新分隔条几何
    void setGeometry(const QRect& geometry) { geometry_ = geometry; }

private:
    BoxNode* container_ = nullptr;
    LayoutNode* side1_ = nullptr;
    LayoutNode* side2_ = nullptr;
    QRect geometry_;
};

}
