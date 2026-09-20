/**
 * @file DockRegion.h
 * @brief 停靠区域：持有布局树根节点，负责命令式停靠与拖放落点
 */

#pragma once

#include "DockObject.h"
#include "DockEnums.h"

#include <QRect>
#include <QSize>

namespace dock {

class DockPanel;
class PanelGroup;
class LayoutNode;

/**
 * @brief 停靠区域
 *
 * 主窗口与浮动窗口各持有一个 DockRegion。命令式停靠由 placePanel()
 * 完成：为面板创建分组与布局节点，并按相对位置插入布局树。
 */
class DockRegion : public DockObject
{
    Q_OBJECT
public:
    explicit DockRegion(QObject* parent = nullptr);
    ~DockRegion() override;

    //! @brief 布局树根节点（可为叶子或容器）
    LayoutNode* rootNode() const { return root_item_; }
    //! @brief 设置根节点（接管所有权，替换旧根）
    void setRootNode(LayoutNode* item);
    //! @brief 摘出根节点但不删除（所有权交还调用方）
    LayoutNode* takeRootNode();

    //! @brief 应用区域几何并重排
    void setGeometry(const QRect& geometry);
    //! @brief 当前区域几何
    QRect geometry() const { return geometry_; }

    //! @brief 设置区域原点在屏幕坐标系中的位置（由视图层同步）
    void setGlobalOrigin(const QPoint& origin) { global_origin_ = origin; }
    //! @brief 区域原点在屏幕坐标系中的位置
    QPoint globalOrigin() const { return global_origin_; }

    /**
     * @brief 命令式添加停靠面板
     * @param panel 目标面板
     * @param location 停靠方向
     * @param relative_to 相对面板（空表示相对整个布局）
     * @param preferred_size 期望尺寸（仅主轴分量生效）
     * @param option 初始可见性（PanelLaunch::Hidden 时不占布局空间）
     */
    void placePanel(DockPanel* panel, DockEdge edge, DockPanel* relative_to,
        const QSize& preferred_size, PanelLaunch launch);

    //! @brief 以选项卡方式加入中央分组
    void stackPanel(DockPanel* panel, PanelGroup* group);

    //! @brief 把分组从本区域摘出用于浮动；创建隐藏占位并返回分组布局节点
    LayoutNode* extractGroupForWindow(PanelGroup* group);
    //! @brief 把浮动的分组放回本区域（用分组节点替换隐藏占位）
    bool restoreGroupFromWindow(PanelGroup* group);
    //! @brief 删除分组在 本区域 的隐藏占位
    void discardGroupVacancy(PanelGroup* group);
    //! @brief 把分组布局节点从本区域树上摘除但不删除（所有权交还调用方）
    bool extractGroupNode(PanelGroup* group);
    //! @brief 把已摘除节点的分组停靠到本区域的指定落点
    //! @note 空 preferred_size 表示无期望尺寸，按公平份额分配（可见子项均分）
    bool attachGroup(PanelGroup* group, DropZone location, PanelGroup* target_group,
        const QSize& preferred_size);

    //! @brief 标记中央持久节点（拖放时不可被替换/移除）
    void setCentralNode(LayoutNode* item) { central_item_ = item; }
    //! @brief 节点是否为中央持久节点（沿父链判断）
    bool isCentralNode(const LayoutNode* item) const;

    //! @brief 按全局坐标命中分组（拖放悬停用）
    PanelGroup* groupAt(const QPoint& global_pos) const;
    //! @brief 区域布局树内的全部分组（树序）
    QList<PanelGroup*> groups() const;

#ifdef QT_DEBUG
    //! @brief 调试用：校验布局树父子一致性与无环，损坏时断言
    void validateTree() const;
#endif

private:
    //! @brief 把节点插入到相对节点旁，必要时包一层同向容器
    void insertRelative(LayoutNode* item, DockEdge edge, LayoutNode* relative_to,
        int preferred_length, bool visible);

    LayoutNode* root_item_ = nullptr;
    LayoutNode* central_item_ = nullptr;
    QRect geometry_;
    QPoint global_origin_;
};

}
