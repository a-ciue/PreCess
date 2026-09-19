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
    Q_PROPERTY(int activeIndex READ activeIndex NOTIFY groupChanged)
    Q_PROPERTY(bool central READ isCentral NOTIFY groupChanged)
    Q_PROPERTY(bool detached READ isDetached NOTIFY groupChanged)
    Q_PROPERTY(bool hasTitleBar READ hasTitleBar NOTIFY groupChanged)

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

    // DockView
    DockObject* dockObject() const override;
    void applyFrame(const QRect& geometry) override;
    QRect frame() const override;
    void applyVisibility(bool visible) override;
    bool isShown() const override;
    QSize minExtent() const override { return QSize(0, 0); }
    QSize maxExtent() const override { return QSize(kMaxSizeLimit, kMaxSizeLimit); }
    void setParentDockView(DockView* parent) override { parent_view_ = parent; }
    DockView* parentDockView() const override { return parent_view_; }
    void bringToFront() override;
    void setCursorShape(Qt::CursorShape shape) override;
    Qt::CursorShape cursorShape() const override;
    QPoint globalOrigin() const override;
    DockView* createDockWindow(DockObject* controller) override;

Q_SIGNALS:
    //! @brief 分组状态（标题/选项卡/浮动等）变化
    void groupChanged();

private:
    //! @brief 按当前面板重挂 client item
    void updateGuest();
    //! @brief 让 client 填满内容宿主
    void layoutGuest();
    //! @brief 所属布局区域
    DockAreaItem* areaItem() const;

    PanelGroup* group_ = nullptr;
    QQuickItem* root_item_ = nullptr;
    QQuickItem* content_area_ = nullptr;
    DockPanel* shown_dock_ = nullptr;
    DragHandle* drag_ = nullptr;
    DockView* parent_view_ = nullptr;
};

}
}
