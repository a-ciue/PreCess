/**
 * @file BoxNode.h
 * @brief 盒式布局容器：子节点沿单一方向排列并可拖拽分隔条调整
 */

#pragma once

#include "LayoutNode.h"

#include <QList>

namespace dock {

class Divider;

/**
 * @brief 盒式布局容器
 *
 * 子节点沿 orientation 指定的主轴依次排列，相邻可见子节点之间自动
 * 维护分隔条。容器尺寸变化或子节点增删/显隐时按主轴占比重新分配长度，
 * 并保证每个子节点不小于其 minExtent。
 */
class BoxNode : public LayoutNode
{
public:
    explicit BoxNode(Qt::Orientation orientation);
    ~BoxNode() override;

    bool isContainer() const override { return true; }

    //! @brief 排列方向（水平容器内子节点自左向右）
    Qt::Orientation orientation() const { return orientation_; }

    //! @brief 全部子节点
    const QList<LayoutNode*>& children() const { return children_; }
    //! @brief 子节点数量
    int childCount() const { return children_.size(); }
    //! @brief 子节点下标，不存在返回 -1
    int indexOfNode(LayoutNode* item) const { return children_.indexOf(item); }
    //! @brief 可见且自身可见的子节点数量
    int visibleNodeCount() const;
    //! @brief 可见子节点列表
    QList<LayoutNode*> visibleNodes() const;

    //! @brief 用 new_child 原地替换 old_child（继承可见性；占比由调用方先行设置）
    void replaceNode(LayoutNode* old_child, LayoutNode* new_child);

    /**
     * @brief 在 index 处插入子节点并接管所有权
     * @param index 插入位置，越界时收敛到两端
     * @param item 被插入的节点
     * @param preferredLength 主轴期望长度（<=0 表示均分）
     * @param startsVisible 插入后是否可见；不可见时记录占比供 show 时恢复
     */
    void insertNode(int index, LayoutNode* item, int preferredLength, bool startsVisible);

    /**
     * @brief 从容器中摘除子节点
     *
     * 若容器只剩一个子节点且自身有父容器，则用该子节点替换本容器
     * （返回替代节点，调用方负责在返回值非 this 时删除本容器）；
     * 若非根容器清空，则从父容器摘除并返回 nullptr。
     * @return 本容器或替代节点；返回 nullptr 表示本容器已空且无父容器
     */
    LayoutNode* detachNode(LayoutNode* item, bool hardDelete);

    void setGeometry(const QRect& geometry) override;
    QSize minExtent() const override;
    QSize maxExtent() const override;

    //! @brief 按主轴占比重算全部可见子节点几何与分隔条位置
    void layout();

    //! @brief 相邻可见子节点之间的分隔条
    const QList<Divider*>& dividers() const { return dividers_; }
    //! @brief 重建分隔条（子节点集合变化后调用）
    void rebuildDividers();
    //! @brief 应用分隔条拖动后的侧 1/侧 2 主轴长度并重排
    void applyDividerMove(Divider* separator, int side1Length, int side2Length);

    //! @brief 子节点可见性变化回调，由 LayoutNode::setVisible 触发
    void handleChildVisibility(LayoutNode* child, bool visible);

private:
    //! @brief 扣除分隔条后的可用主轴长度
    int freeSpan() const;
    //! @brief 给新插入的可见子节点分配期望长度（其余可见子节点按比例让位）
    void assignPreferredSpan(LayoutNode* item, int preferredLength);
    //! @brief 子节点可见性变化后同步容器自身的有效可见性
    void syncVisibility();

    Qt::Orientation orientation_;
    QList<LayoutNode*> children_;
    QList<Divider*> dividers_;
};

}
