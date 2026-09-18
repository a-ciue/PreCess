/**
 * @file Separator.h
 * @brief 相邻可见布局项之间的可拖动分隔条
 */

#pragma once

#include <QRect>

namespace dock {

class Item;
class ItemBoxContainer;

/**
 * @brief 分隔条
 *
 * 由 ItemBoxContainer::updateSeparators() 在相邻可见子节点之间创建，
 * 拖动时在两侧子节点最小尺寸约束内移动，并触发容器按占比重排。
 */
class Separator
{
public:
    Separator(ItemBoxContainer* container, Item* side1, Item* side2);

    //! @brief 所属容器
    ItemBoxContainer* container() const { return container_; }
    //! @brief 前一个可见子节点
    Item* side1() const { return side1_; }
    //! @brief 后一个可见子节点
    Item* side2() const { return side2_; }

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
    ItemBoxContainer* container_ = nullptr;
    Item* side1_ = nullptr;
    Item* side2_ = nullptr;
    QRect geometry_;
};

}
