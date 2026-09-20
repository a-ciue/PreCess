/**
 * @file DockHost.h
 * @brief 主停靠窗口：持有根停靠区域与中央持久面板
 */

#pragma once

#include "DockObject.h"
#include "DockEnums.h"

#include <QString>

namespace dock {

class DockPanel;
class DockRegion;
class PanelGroup;
class LayoutNode;
class DockView;

/**
 * @brief 主停靠窗口
 *
 * 一个应用当前仅使用一个 DockHost（对应 QML 的 DockHost）。
 * 构造时创建中央持久面板：不可隐藏、不可拆出、不可拖动。
 */
class DockHost : public DockObject
{
    Q_OBJECT
public:
    explicit DockHost(const QString& unique_name, QObject* parent = nullptr);
    ~DockHost() override;

    //! @brief 唯一名称
    QString uniqueName() const { return unique_name_; }

    //! @brief 根停靠区域
    DockRegion* region() const { return region_; }

    //! @brief 中央持久面板
    DockPanel* centralPanel() const { return central_panel_; }
    //! @brief 中央分组
    PanelGroup* centralGroup() const { return central_group_; }
    //! @brief 中央布局节点
    LayoutNode* centralNode() const { return central_node_; }

    //! @brief 设置中央内容视图（由视图层加载 QML 后注入）
    void setCentralContentView(DockView* content_view);

    /**
     * @brief 命令式放置面板
     * @param panel 目标面板
     * @param edge 停靠方位
     * @param relative_to 相对面板（空表示相对整个布局）
     * @param preferred_size 期望尺寸（仅主轴分量生效）
     * @param launch 初始可见性（Hidden 时不占布局空间）
     * @sa DockRegion::placePanel
     */
    void placePanel(DockPanel* panel, DockEdge edge, DockPanel* relative_to,
        const QSize& preferred_size, PanelLaunch launch = PanelLaunch::Visible);

    //! @brief 以选项卡方式加入中央分组
    void stackPanel(DockPanel* panel);

    //! @brief 隐藏除指定分组外的所有显示面板（中央持久部件不受影响）
    void hideOtherGroups(PanelGroup* except);

    //! @brief 应用窗口内容区几何（视图层尺寸变化时调用）
    void setFrame(const QRect& frame);

Q_SIGNALS:
    //! @brief 布局尺寸变化
    void frameChanged(const QRect& frame);

private:
    QString unique_name_;
    DockRegion* region_ = nullptr;
    DockPanel* central_panel_ = nullptr;
    PanelGroup* central_group_ = nullptr;
    LayoutNode* central_node_ = nullptr;
};

}
