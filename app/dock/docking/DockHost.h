/**
 * @file DockHost.h
 * @brief 主停靠窗口：持有根停靠区域与中央持久面板
 */

#pragma once

#include "DockObject.h"
#include "DockEnums.h"

#include <QByteArray>
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
    //! @brief 中央布局节点（布局快照恢复复用该节点）
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

    //! @brief 隐藏除指定分组外的所有显示面板（中央持久部件不受影响）
    void hideOtherGroups(PanelGroup* except);

    //! @brief 应用窗口内容区几何（视图层尺寸变化时调用）
    void setFrame(const QRect& frame);

    /**
     * @brief 序列化当前布局（JSON，含主区域树与浮动窗口）
     *
     * 仅保存面板 uniqueName 的顺序/显隐/激活与树结构、占比、浮窗几何；
     * 标题与能力位等内容由声明层负责，不进入快照。
     * @note 非 const：导出前会取消进行中的拖拽（会话中的临时浮窗不属于稳定布局），
     *       调用方应视作有副作用的操作而非只读查询
     */
    QByteArray saveLayout();

    /**
     * @brief 从布局快照恢复
     *
     * 版本/宿主/中央校验失败、数据畸形或主树无法解析出中央分组时，返回 false
     * 且不改动当前布局；未知面板名跳过、快照未包含的面板保持当前默认位置
     * （向前/向后兼容）。
     */
    bool restoreLayout(const QByteArray& layout);

    /**
     * @brief 清空为仅含中央面板的默认空布局
     *
     * 销毁全部浮动窗口与其余分组（面板对象保留并置为隐藏），中央面板保持显示；
     * 供 restoreLayout() 重建前的清理使用。
     */
    void clearLayout();

Q_SIGNALS:
    //! @brief 布局被整体恢复（视图层需重新同步）
    void layoutRestored();

private:
    //! @brief 清空布局的实现；notify 为 false 时（恢复流程内部调用）不发 layoutRestored
    void clearLayoutInternal(bool notify);

    QString unique_name_;
    DockRegion* region_ = nullptr;
    DockPanel* central_panel_ = nullptr;
    PanelGroup* central_group_ = nullptr;
    LayoutNode* central_node_ = nullptr;
};

}
