/**
 * @file IndicatorsView.h
 * @brief 拖放落点高亮视图
 */

#pragma once

#include <QQuickItem>
#include <QRect>

namespace dock::qtquick {

/**
 * @brief 落点高亮视图
 *
 * 由 AreaItem 在拖拽悬停时设置单个高亮矩形；QML 只负责绘制。
 */
class IndicatorsView : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY indicatorsChanged)
    Q_PROPERTY(int highlightX READ highlightX NOTIFY indicatorsChanged)
    Q_PROPERTY(int highlightY READ highlightY NOTIFY indicatorsChanged)
    Q_PROPERTY(int highlightWidth READ highlightWidth NOTIFY indicatorsChanged)
    Q_PROPERTY(int highlightHeight READ highlightHeight NOTIFY indicatorsChanged)

public:
    explicit IndicatorsView(QQuickItem* parent = nullptr);
    ~IndicatorsView() override;

    //! @brief 显示高亮矩形（区域局部坐标）
    void setHighlight(const QRect& rect);
    //! @brief 清除高亮
    void clearHighlight();

    bool isActive() const { return !highlight_.isNull(); }
    int highlightX() const { return highlight_.x(); }
    int highlightY() const { return highlight_.y(); }
    int highlightWidth() const { return highlight_.width(); }
    int highlightHeight() const { return highlight_.height(); }

Q_SIGNALS:
    //! @brief 高亮变化
    void indicatorsChanged();

private:
    QRect highlight_;
};

}
