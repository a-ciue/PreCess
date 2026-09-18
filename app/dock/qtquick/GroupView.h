/**
 * @file GroupView.h
 * @brief 分组视图：QML 视图对象与 Group 控制器之间的桥
 */

#pragma once

#include "core/View.h"
#include "engine/SizingInfo.h"

#include <QQuickItem>
#include <QStringList>

namespace dock {

class DockWidget;
class Draggable;
class Group;

namespace qtquick {

class AreaItem;

/**
 * @brief 分组视图
 *
 * 由 Platform 按 Group 创建并缓存；在布局区域之间移动时复用同一视图
 * （guest item 随当前面板重挂到内容宿主下）。
 */
class GroupView : public QQuickItem, public View
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title NOTIFY groupChanged)
    Q_PROPERTY(QStringList tabTitles READ tabTitles NOTIFY groupChanged)
    Q_PROPERTY(int tabCount READ tabCount NOTIFY groupChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY groupChanged)
    Q_PROPERTY(bool central READ isCentral NOTIFY groupChanged)
    Q_PROPERTY(bool floating READ isFloating NOTIFY groupChanged)
    Q_PROPERTY(bool titleBarVisible READ titleBarVisible NOTIFY groupChanged)

public:
    explicit GroupView(Group* group, QQuickItem* parent = nullptr);
    ~GroupView() override;

    //! @brief 关联的分组控制器
    Group* group() const { return group_; }

    //! @brief 同步 QML 属性并刷新 guest（可重复调用，用于 guest 延迟注册）
    void syncFromGroup();

    // QML 成员函数
    Q_INVOKABLE void setCurrentIndex(int index);
    Q_INVOKABLE void closeGroup();
    Q_INVOKABLE void toggleFloat();
    //! @brief 标题栏拖拽：拖动整个分组
    Q_INVOKABLE void beginDrag(const QPointF& global_pos);
    //! @brief 标签拖拽：只分离该标签面板（index 为打开面板下标）
    Q_INVOKABLE void beginTabDrag(int index, const QPointF& global_pos);
    Q_INVOKABLE void dragTo(const QPointF& global_pos);
    Q_INVOKABLE void endDrag(const QPointF& global_pos);

    // 属性读取
    QString title() const;
    QStringList tabTitles() const;
    int tabCount() const;
    int currentIndex() const;
    bool isCentral() const;
    bool isFloating() const;
    bool titleBarVisible() const;

    // View
    Controller* controller() const override;
    void setViewGeometry(const QRect& geometry) override;
    QRect viewGeometry() const override;
    void setViewVisible(bool visible) override;
    bool isViewVisible() const override;
    QSize viewMinSize() const override { return QSize(0, 0); }
    QSize viewMaxSizeHint() const override { return QSize(kMaxSizeLimit, kMaxSizeLimit); }
    void setParentView(View* parent) override { parent_view_ = parent; }
    View* parentView() const override { return parent_view_; }
    void raiseView() override;
    void setViewCursor(Qt::CursorShape shape) override;
    Qt::CursorShape viewCursor() const override;
    QPoint viewGlobalPosition() const override;
    View* createFloatingWindowView(Controller* controller) override;

Q_SIGNALS:
    //! @brief 分组状态（标题/选项卡/浮动等）变化
    void groupChanged();

private:
    //! @brief 按当前面板重挂 guest item
    void updateGuest();
    //! @brief 让 guest 填满内容宿主
    void layoutGuest();
    //! @brief 所属布局区域
    AreaItem* areaItem() const;

    Group* group_ = nullptr;
    QQuickItem* root_item_ = nullptr;
    QQuickItem* content_area_ = nullptr;
    DockWidget* shown_dock_ = nullptr;
    Draggable* drag_ = nullptr;
    View* parent_view_ = nullptr;
};

}
}
