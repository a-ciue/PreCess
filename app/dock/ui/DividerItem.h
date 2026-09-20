/**
 * @file DividerItem.h
 * @brief 分隔条视图：把鼠标拖动转成引擎的分隔条移动
 */

#pragma once

#include <QQuickItem>

namespace dock {

class Divider;

namespace ui {

class DockAreaItem;

class DividerItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(bool horizontal READ isHorizontal NOTIFY separatorChanged)

public:
    explicit DividerItem(QQuickItem* parent = nullptr);
    ~DividerItem() override;

    //! @brief 绑定引擎分隔条与所属区域
    void setDivider(Divider* separator, DockAreaItem* area);
    //! @brief 容器是否为水平方向（对应水平拖动光标）
    bool isHorizontal() const;

    Q_INVOKABLE void beginGroupDrag(const QPointF& global_pos);
    Q_INVOKABLE void dragTo(const QPointF& global_pos);
    Q_INVOKABLE void endDrag();

Q_SIGNALS:
    //! @brief 分隔条绑定变化
    void separatorChanged();

private:
    //! @brief 全局坐标换算为容器主轴坐标
    int mainAxisPosition(const QPointF& global_pos) const;

    Divider* divider_ = nullptr;
    DockAreaItem* area_ = nullptr;
    bool dragging_ = false;
};

}
}
