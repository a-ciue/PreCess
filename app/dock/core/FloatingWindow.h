/**
 * @file FloatingWindow.h
 * @brief 浮动窗口：承载被拖出主窗口的分组
 */

#pragma once

#include "Controller.h"

#include <QRect>

namespace dock {

class DropArea;
class Group;
class Item;

/**
 * @brief 浮动窗口
 *
 * 每个浮动窗口持有一个 DropArea（支持在浮窗内继续停靠）。
 * 视图层负责实际顶层窗口（无边框 QQuickWindow）的创建与边缘缩放。
 * 核心层拥有 FloatingWindow 对象；关闭时发出 closed() 通知视图层销毁窗口。
 */
class FloatingWindow : public Controller
{
    Q_OBJECT
public:
    explicit FloatingWindow(QObject* parent = nullptr);
    ~FloatingWindow() override;

    //! @brief 浮动窗口内的停靠区域
    DropArea* dropArea() const { return drop_area_; }

    //! @brief 当前承载的分组（简化：单分组）
    Group* group() const { return group_; }

    //! @brief 接管分组与它的布局节点（接管节点所有权）
    void takeGroup(Group* group, Item* item);
    //! @brief 释放分组但保留布局节点（节点被摘出，所有权交还调用方）
    Item* releaseGroup();

    //! @brief 是否未承载任何分组
    bool isEmpty() const;

    //! @brief 窗口几何（全局屏幕坐标）
    QRect geometry() const { return geometry_; }
    //! @brief 设置窗口几何（由视图层或拖放逻辑调用）
    void setGeometry(const QRect& geometry);

    //! @brief 标题
    QString title() const;

    //! @brief 通知视图层关闭窗口（核心对象由创建方删除）
    void close();

Q_SIGNALS:
    //! @brief 标题变化
    void titleChanged(const QString& title);
    //! @brief 几何变化（视图层同步窗口位置/尺寸）
    void geometryChanged(const QRect& geometry);
    //! @brief 窗口关闭
    void closed();

private:
    DropArea* drop_area_ = nullptr;
    Group* group_ = nullptr;
    QRect geometry_;
};

}
