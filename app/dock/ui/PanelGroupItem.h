/**
 * @file PanelGroupItem.h
 * @brief 分组视图：QML 视图对象与 PanelGroup 控制器之间的桥
 */

#pragma once

#include "docking/DockView.h"
#include "tree/NodeMetrics.h"

#include <QQuickItem>
#include <QStringList>

namespace dock {

class DockPanel;
class DragHandle;
class PanelGroup;

namespace ui {

class DockAreaItem;

/**
 * @brief 分组视图
 *
 * 由 DockRuntime 按 PanelGroup 创建并缓存；在布局区域之间移动时复用同一视图
 * （client item 随当前面板重挂到内容宿主下）。
 */
class PanelGroupItem : public QQuickItem, public DockView
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY groupChanged)
    Q_PROPERTY(QStringList tabNames READ tabNames NOTIFY groupChanged)
    Q_PROPERTY(int tabCount READ tabCount NOTIFY groupChanged)
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY activeTabChanged)
    Q_PROPERTY(bool central READ isCentral NOTIFY groupChanged)
    Q_PROPERTY(bool detached READ isDetached NOTIFY groupChanged)
    Q_PROPERTY(bool hasTitleBar READ hasTitleBar NOTIFY groupChanged)
    Q_PROPERTY(bool closable READ isClosable NOTIFY groupChanged)
    Q_PROPERTY(bool movable READ isMovable NOTIFY groupChanged)
    Q_PROPERTY(bool floatable READ isFloatable NOTIFY groupChanged)
    Q_PROPERTY(bool reordering READ isReordering NOTIFY reorderChanged)
    Q_PROPERTY(int reorderMarkerX READ reorderMarkerX NOTIFY reorderChanged)
    Q_PROPERTY(bool updatingTabs READ isUpdatingTabs)

public:
    explicit PanelGroupItem(PanelGroup* group, QQuickItem* parent = nullptr);
    ~PanelGroupItem() override;

    //! @brief 关联的分组控制器
    PanelGroup* group() const { return group_; }

    //! @brief 同步 QML 属性并刷新 client（可重复调用，用于 client 延迟注册）
    void syncFromGroup();

    // QML 成员函数
    Q_INVOKABLE void activateTab(int index);
    Q_INVOKABLE void hideGroup();
    //! @brief 关闭第 index 个显示标签（受可关闭能力约束）
    Q_INVOKABLE void hidePanelAt(int index);
    //! @brief 关闭除第 index 个显示标签外的组内标签
    Q_INVOKABLE void hideOthers(int index);
    //! @brief 关闭其他分组（等价 ADS closeOtherAreas）
    Q_INVOKABLE void hideOtherGroups();
    Q_INVOKABLE void toggleDetached();
    //! @brief 标题栏拖拽：拖动整个分组
    Q_INVOKABLE void beginGroupDrag(const QPointF& global_pos);
    //! @brief 标签拖拽：只分离该标签面板（index 为打开面板下标）
    Q_INVOKABLE void beginPanelDrag(int index, const QPointF& global_pos);
    Q_INVOKABLE void dragTo(const QPointF& global_pos);
    Q_INVOKABLE void endDrag(const QPointF& global_pos);

    // 属性读取
    QString title() const;
    QStringList tabNames() const;
    int tabCount() const;
    int activeIndex() const;
    bool isCentral() const;
    bool isDetached() const;
    bool hasTitleBar() const;
    //! @brief 组内显示面板是否可关闭（能力位交集）
    bool isClosable() const;
    //! @brief 组内显示面板是否可拖动
    bool isMovable() const;
    //! @brief 组内显示面板是否可浮动
    bool isFloatable() const;
    //! @brief 是否处于组内标签重排预览（拖拽标签横向移动）
    bool isReordering() const { return reordering_; }
    //! @brief 重排插入标记在视图内的 x 坐标（无效时 -1）
    int reorderMarkerX() const;
    //! @brief 是否正在同步选项卡模型（模型重建期间抑制 TabBar 下标回调）
    bool isUpdatingTabs() const { return updating_tabs_; }

    //! @brief 标签插入位置查询（DockView）：命中标题栏/标签栏条带时返回 0..count，否则 -1
    int tabInsertIndexAt(const QPoint& global_pos) const override;
    //! @brief 插入标记矩形（视图局部坐标；无效时为空）
    QRect tabInsertMarkerRect(int index) const;

    // DockView
    DockObject* dockObject() const override;
    void applyFrame(const QRect& geometry) override;
    QRect frame() const override;
    void applyVisibility(bool visible) override;
    bool isShown() const override;
    QSize minExtent() const override { return QSize(0, 0); }
    QSize maxExtent() const override { return QSize(kMaxSizeLimit, kMaxSizeLimit); }
    void bringToFront() override;
    QPoint globalOrigin() const override;
    DockView* createDockWindow(DockObject* controller) override;

Q_SIGNALS:
    //! @brief 分组状态（标题/选项卡/浮动等）变化
    void groupChanged();
    //! @brief 当前选项卡变化（不重建标签模型，避免按下未激活标签时销毁其委托）
    void activeTabChanged();
    //! @brief 组内标签重排预览状态变化
    void reorderChanged();

private:
    //! @brief 按当前面板重挂 client item
    void updateGuest();
    //! @brief 让 client 填满内容宿主
    void layoutGuest();
    //! @brief 所属布局区域
    DockAreaItem* areaItem() const;
    //! @brief 标签栏第 index 个标签按钮（越界或不可用时为空）
    QQuickItem* tabItem(int index) const;
    //! @brief 校正 TabBar 当前下标与核心激活面板一致
    void syncTabIndex();
    //! @brief 清除组内重排预览状态
    void clearReorderPreview();

    PanelGroup* group_ = nullptr;
    QQuickItem* root_item_ = nullptr;
    QQuickItem* content_area_ = nullptr;
    QQuickItem* tab_bar_ = nullptr;
    QQuickItem* title_bar_ = nullptr;
    DockPanel* shown_dock_ = nullptr;
    DragHandle* drag_ = nullptr;

    // 组内标签重排：按下后先判别轴向，横向留在标签栏内为重排，纵向超阈值转浮动
    QPointF press_global_;
    int reorder_from_ = -1;
    int reorder_marker_index_ = -1;
    bool reordering_ = false;
    bool updating_tabs_ = false;
};

}
}
