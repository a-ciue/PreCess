/**
 * @file Item.h
 * @brief 布局树节点
 */

#pragma once

#include "SizingInfo.h"

#include <QString>

namespace dock {

class ItemBoxContainer;
class LayoutingGuest;

/**
 * @brief 布局树节点（叶子节点与容器节点的公共基类）
 *
 * 叶子节点持有 LayoutingGuest，几何变化经 guest 接口下发；
 * 容器节点由 ItemBoxContainer 实现。主轴占比 percentage 描述节点在
 * 父容器主轴上的长度份额，父容器尺寸变化时按占比重新分配长度。
 */
class Item
{
public:
    explicit Item(LayoutingGuest* guest = nullptr);
    virtual ~Item();

    //! @brief 是否为容器节点
    virtual bool isContainer() const { return false; }

    //! @brief 布局几何（父容器坐标系）
    const QRect& geometry() const { return sizing_.geometry; }
    //! @brief 设置几何并下发给 guest
    virtual void setGeometry(const QRect& geometry);

    //! @brief 自身可见性（不含父链影响）
    virtual bool isVisible() const { return visible_; }
    //! @brief 设置可见性；变化时通知父容器重排
    virtual void setVisible(bool visible);
    //! @brief 设置可见性但不通知父容器（仅用于树结构调整过程中）
    void setVisibleSilently(bool visible);

    //! @brief 主轴方向上的长度
    int length(Qt::Orientation orientation) const { return sizing_.length(orientation); }
    //! @brief 主轴方向上的起点坐标
    int position(Qt::Orientation orientation) const { return sizing_.position(orientation); }

    //! @brief 硬最小尺寸（默认取 guest 约束）
    virtual QSize minSize() const;
    //! @brief 软最大尺寸提示（默认取 guest 约束）
    virtual QSize maxSizeHint() const;

    //! @brief 主轴长度占比（0.0~1.0）
    double percentage() const { return sizing_.percentage; }
    //! @brief 设置主轴占比；几何在容器 layout() 时更新
    void setPercentage(double percentage) { sizing_.percentage = percentage; }

    //! @brief 隐藏前保存的占比，重新显示时用于恢复
    double storedPercentage() const { return stored_percentage_; }
    //! @brief 设置隐藏前保存的占比
    void setStoredPercentage(double percentage) { stored_percentage_ = percentage; }

    //! @brief 父容器
    ItemBoxContainer* parent() const { return parent_; }
    //! @brief 设置父容器（由容器维护，不应手工调用）
    void setParent(ItemBoxContainer* parent) { parent_ = parent; }

    //! @brief 被托管的实体（容器节点为空）
    LayoutingGuest* guest() const { return guest_; }
    //! @brief 设置被托管的实体
    void setGuest(LayoutingGuest* guest) { guest_ = guest; }

    //! @brief 调试标识
    const QString& id() const { return id_; }
    //! @brief 设置调试标识
    void setId(const QString& id) { id_ = id; }

protected:
    //! @brief 几何与尺寸约束
    SizingInfo sizing_;
    //! @brief 被托管的实体（容器节点为空）
    LayoutingGuest* guest_ = nullptr;
    //! @brief 父容器
    ItemBoxContainer* parent_ = nullptr;
    //! @brief 自身可见性（不含父链影响）
    bool visible_ = true;

private:
    double stored_percentage_ = 0.0;
    QString id_;
};

}
