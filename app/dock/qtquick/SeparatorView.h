/**
 * @file SeparatorView.h
 * @brief 分隔条视图：把鼠标拖动转成引擎的分隔条移动
 */

#pragma once

#include <QQuickItem>

namespace dock {

class Separator;

namespace qtquick {

class AreaItem;

class SeparatorView : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(bool horizontal READ isHorizontal NOTIFY separatorChanged)

public:
    explicit SeparatorView(QQuickItem* parent = nullptr);
    ~SeparatorView() override;

    //! @brief 绑定引擎分隔条与所属区域
    void setSeparator(Separator* separator, AreaItem* area);
    //! @brief 绑定的分隔条
    Separator* separator() const { return separator_; }
    //! @brief 容器是否为水平方向（对应水平拖动光标）
    bool isHorizontal() const;

    Q_INVOKABLE void beginDrag(const QPointF& global_pos);
    Q_INVOKABLE void dragTo(const QPointF& global_pos);
    Q_INVOKABLE void endDrag();

Q_SIGNALS:
    //! @brief 分隔条绑定变化
    void separatorChanged();

private:
    //! @brief 全局坐标换算为容器主轴坐标
    int mainAxisPosition(const QPointF& global_pos) const;

    Separator* separator_ = nullptr;
    AreaItem* area_ = nullptr;
    bool dragging_ = false;
};

}
}
