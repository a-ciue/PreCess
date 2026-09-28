/**
 * @file PanelGroup.h
 * @brief 选项卡分组：承载一个或多个停靠面板的布局单元
 */

#pragma once

#include "DockObject.h"
#include "DockPanel.h"
#include "tree/LayoutClient.h"

#include <QList>
#include <QSize>

namespace dock {

class LayoutNode;

/**
 * @brief 选项卡分组
 *
 * 分组是布局树的叶子实体（LayoutClient）：可见性由组内是否有显示的
 * 面板决定；全部面板隐藏时分组整体让出布局空间但保留其在树中的位置，
 * 面板重新显示时恢复原位置与占比。
 */
class PanelGroup : public DockObject, public LayoutClient
{
    Q_OBJECT
public:
    explicit PanelGroup(QObject* parent = nullptr);
    ~PanelGroup() override;

    //! @brief 追加面板（不改变显示状态）
    void addPanel(DockPanel* panel);
    //! @brief 在显示面板序列的指定位置插入面板（0..显示面板数；越界按追加处理）
    void insertPanel(DockPanel* panel, int shown_index);
    //! @brief 在显示面板序列内移动面板（from/to 为显示面板下标，to 为插入位置）
    bool movePanel(int from, int to);
    //! @brief 移除面板（当前仅用于对象销毁前的解绑）
    void removePanel(DockPanel* panel);

    //! @brief 组内全部面板（含已隐藏）
    const QList<DockPanel*>& panels() const { return panels_; }
    //! @brief 组内显示的面板
    QList<DockPanel*> shownPanels() const;

    //! @brief 当前选项卡面板（可能为空）
    DockPanel* activePanel() const { return active_; }
    //! @brief 设为当前选项卡（面板须在组内且已显示）
    void setActivePanel(DockPanel* panel);
    //! @brief 当前选项卡在显示面板中的下标
    int activeIndex() const;
    //! @brief 按显示面板下标切换
    void setActiveIndex(int index);

    //! @brief 隐藏除指定面板外的组内显示面板
    void hideOthers(DockPanel* panel);
    //! @brief 组内显示面板的能力位交集（中央分组恒为无能力）
    DockPanel::Features features() const;

    //! @brief 分组标题（当前面板标题）
    QString title() const;

    //! @brief 是否为中央持久分组
    bool isCentral() const { return central_; }
    //! @brief 标记为中央持久分组
    void setIsCentral(bool central) { central_ = central; }

    //! @brief 布局树中的节点
    LayoutNode* node() const { return node_; }
    //! @brief 设置布局节点（由布局层创建后注入）
    void setNode(LayoutNode* node) { node_ = node; }

    //! @brief 拆出期间留在主布局中的隐藏占位节点
    LayoutNode* vacancy() const { return vacancy_; }
    //! @brief 设置占位节点（由布局层维护）
    void setVacancy(LayoutNode* node) { vacancy_ = node; }

    //! @brief 根据显示面板情况同步分组可见性
    void syncVisibility();

    //! @brief 同步组内面板的拆出状态
    void setDetached(bool detached);

    // LayoutClient
    void applyGeometry(const QRect& geometry) override;
    QSize minExtent() const override;
    QSize maxExtent() const override;
    void applyVisibility(bool visible) override;

Q_SIGNALS:
    //! @brief 面板集合或显示状态变化
    void panelsChanged();
    //! @brief 当前选项卡变化
    void activePanelChanged(DockPanel* panel);
    //! @brief 分组标题变化
    void titleChanged(const QString& title);

private:
    //! @brief 当前面板不可用时选择下一个可显示的面板
    void refreshActivePanel();
    //! @brief 在组内面板序列的指定下标插入（公共接口的位置映射终点）
    void insertPanelAt(DockPanel* panel, int panels_index);
    /**
     * @brief 把面板从组内列表剔除并修正激活/可见性
     * @note 不访问面板成员；供正常移除路径（removePanel）复用
     */
    void forgetPanel(DockPanel* panel);

    QList<DockPanel*> panels_;
    DockPanel* active_ = nullptr;
    LayoutNode* node_ = nullptr;
    LayoutNode* vacancy_ = nullptr;
    bool central_ = false;
};

}
