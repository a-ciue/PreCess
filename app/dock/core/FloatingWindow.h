/**
 * @file FloatingWindow.h
 * @brief 浮动窗口：承载被拖出主窗口的分组
 */

#pragma once

#include "Controller.h"

namespace dock {

class DropArea;
class Group;
class Item;

/**
 * @brief 浮动窗口
 *
 * 每个浮动窗口持有一个 DropArea（支持在浮窗内继续停靠）。
 * 视图层负责实际顶层窗口（无边框 QQuickWindow）的创建与边缘缩放。
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
    //! @brief 接管分组
    void setGroup(Group* group);

    //! @brief 标题
    QString title() const;

    //! @brief 关闭浮动窗口（把分组内容交还调用方处理）
    void close();

Q_SIGNALS:
    //! @brief 窗口关闭
    void closed();

private:
    DropArea* drop_area_ = nullptr;
    Group* group_ = nullptr;
    Item* group_item_ = nullptr;
};

}
