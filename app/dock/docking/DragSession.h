/**
 * @file DragSession.h
 * @brief 拖拽状态机：按下 → 超过阈值转拖拽 → 悬停落点 → 释放/取消
 */

#pragma once

#include "DropTarget.h"
#include "DockEnums.h"

#include <QHash>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QSize>

namespace dock {

class DockPanel;
class DragHandle;
class DockRegion;
class DockWindow;
class PanelGroup;
class LayoutNode;
class DragProxy;

/**
 * @brief 拖拽状态机
 *
 * 视图层（全局鼠标过滤器 / QML 标题栏）在按下、移动、释放时驱动本控制器；
 * 拖拽开始时把分组（或单个标签面板）摘入浮动窗口，释放时按悬停落点停靠
 * 或保留浮动。本控制器为核心层单例，不依赖 QtQuick。
 */
class DragSession : public QObject
{
    Q_OBJECT
public:
    //! @brief 状态
    enum class Phase {
        Idle, //!< 空闲
        Armed, //!< 已按下但未超过拖拽阈值
        Dragging //!< 拖拽中
    };
    Q_ENUM(Phase)

    //! @brief 单例
    static DragSession& self();

    //! @brief 当前状态
    Phase phase() const { return phase_; }
    //! @brief 当前拖拽源（空闲时为空）
    DragHandle* handle() const { return handle_; }
    //! @brief 当前被拖拽的浮动窗口（空闲时为空）
    DockWindow* dragWindow() const;

    //! @brief 悬停命中的停靠区域
    DockRegion* hoveredRegion() const { return hover_.region; }
    //! @brief 悬停命中的分组（外落点为空）
    PanelGroup* hoveredGroup() const { return hover_.group; }
    //! @brief 当前悬停落点
    DropZone hoveredZone() const { return hover_.zone; }
    //! @brief 悬停分组的标签插入位置（-1 表示无效）
    int hoveredTabIndex() const { return hover_.tab_index; }

    //! @brief 视图层在标题栏/标签按下时调用
    void beginAt(DragHandle* handle, const QPoint& global_pos);
    //! @brief 视图层转发鼠标移动
    void updateAt(const QPoint& global_pos);
    //! @brief 视图层转发鼠标释放
    void endAt(const QPoint& global_pos);
    //! @brief 取消当前按下/拖拽（Esc、窗口失焦等）
    void cancel();
    /**
     * @brief 静默中止会话（宿主拆解期调用）
     *
     * 只重置会话状态并释放拖拽代理，不移动/回收任何模型对象、不发信号：
     * 此时布局对象可能已处于销毁过程中，触碰它们会造成二次破坏。
     */
    void abort();

    //! @brief 使分组浮动（浮动按钮/浮动菜单）；成功返回 true
    bool detachGroup(PanelGroup* group);
    //! @brief 使浮动分组回停：有占位则回原位，单标签浮出则归还源分组
    bool reattachGroup(PanelGroup* group);
    //! @brief 切换分组浮动/停靠状态
    bool toggleDetached(PanelGroup* group);
    //! @brief 分组是否处于浮动状态（含停靠在浮窗内的次级分组）
    static bool isFloating(PanelGroup* group);

    //! @brief 创建浮动窗口（含视图与空窗监听）；拖拽与布局恢复共用
    DockWindow* createFloatingWindow();
    //! @brief 销毁浮动窗口（布局恢复/清理用：先回收窗内分组再销毁）
    void destroyFloatingWindow(DockWindow* window);

    //! @brief 查询单标签浮停的归还来源（无来源返回 false）
    bool parkedOrigin(PanelGroup* group, PanelGroup*& origin, int& index) const;
    //! @brief 恢复单标签浮停的归还来源（仅供布局恢复）
    void restoreParkedGroup(PanelGroup* group, PanelGroup* origin, int index);

Q_SIGNALS:
    //! @brief 状态变化
    void phaseChanged(Phase phase);
    //! @brief 悬停落点变化（视图层刷新指示器）
    void zoneChanged();
    //! @brief 命令式布局变化（浮动/回停），视图层需重新同步
    void layoutChanged();

private:
    explicit DragSession(QObject* parent = nullptr);
    ~DragSession() override;

    void startDrag(const QPoint& global_pos);
    void updateHover(const QPoint& global_pos);
    //! @brief 应用悬停落点；落点未被消费（目标无效）时返回 false
    bool applyDrop();
    void cleanup();
    //! @brief 重置会话成员（拖拽代理/句柄/来源/临时分组等）；不发信号、不触碰模型对象
    void resetSessionMembers();
    //! @brief 保留浮动：临时组登记回停来源并归属浮动窗口
    void parkFloatingGroup(DockWindow* window);

    /**
     * @brief 摘出分组节点（统一入口），所有权交还调用方
     *
     * 优先级：浮窗主分组（releaseGroup，多分组浮窗只摘叶子）→ 所属区域
     * （根节点 takeRootNode，叶子 extractGroupNode）→ 直接返回 node。
     * 调用方无需区分节点当前挂在主区域还是浮窗内。
     */
    static LayoutNode* takeGroupNode(PanelGroup* group, DockWindow* window);
    //! @brief 删除分组节点（含从布局树摘除）；分组本身保留
    static void disposeGroupNode(PanelGroup* group, DockWindow* window);

    //! @brief 重绑浮窗内全部分组的监听（分组/区域变化时调用）
    void refreshFloatingWatcher(DockWindow* window);
    //! @brief 重绑全部浮窗的监听
    void refreshAllFloatingWatchers();

    //! @brief 仅在浮窗不再承载任何分组时销毁窗口
    static void closeWindowIfEmpty(DockWindow* window);
    //! @brief 整窗回收：把浮窗内全部分组归还主区域后关闭
    void evacuateWindow(DockWindow* window);

    //! @brief 创建浮动窗口核心对象并通知视图层创建窗口
    DockWindow* createDragWindow();
    //! @brief 关闭并销毁浮动窗口核心对象
    static void destroyDragWindow(DockWindow* window);
    //! @brief 查找承载指定分组的浮动窗口
    static DockWindow* windowForGroup(PanelGroup* group);
    //! @brief 查找分组所在的停靠区域（主窗口或浮动窗口）
    static DockRegion* regionForGroup(PanelGroup* group);

    //! @brief 单个标签浮出后的回停来源
    struct FloatOrigin {
        QPointer<PanelGroup> origin_group;
        int index = -1;
    };

    Phase phase_ = Phase::Idle;
    DragHandle* handle_ = nullptr;
    DragProxy* drag_proxy_ = nullptr;
    DockRegion* source_region_ = nullptr;
    DropTarget hover_;
    QPoint press_pos_;

    // 单个标签拖拽
    QPointer<DockPanel> dragged_panel_;
    PanelGroup* drag_group_ = nullptr; // 实际被拖拽的分组（临时组或源分组）
    QPointer<PanelGroup> origin_group_;
    int origin_index_ = -1;

    // 单标签浮出且未停靠时的回停来源（键为临时分组）
    QHash<PanelGroup*, FloatOrigin> floating_origins_;

    // 浮窗分组监听连接（键为浮窗；分组/区域变化时重绑）
    QHash<DockWindow*, QList<QMetaObject::Connection>> floating_watchers_;
    //! @brief 空窗回收处理中（防止嵌套 panelsChanged 重入）
    bool resolving_empty_ = false;
};

}
