/**
 * @file DockWidget.h
 * @brief 逻辑停靠面板：可显示/隐藏、可停靠、可浮动
 */

#pragma once

#include "Controller.h"

#include <QString>

namespace dock {

class Group;
class View;

/**
 * @brief 逻辑停靠面板
 *
 * 面板内容由视图层提供的 guest 视图承载。生命周期状态：
 * Hidden（不占布局空间，可恢复到所属分组的原位置）/ Docked / Floating。
 * 关闭（close）不销毁面板，仅隐藏；open() 恢复显示。
 */
class DockWidget : public Controller
{
    Q_OBJECT
public:
    //! @brief 生命周期状态
    enum class State {
        Hidden, //!< 已关闭
        Docked, //!< 停靠在布局中
        Floating //!< 浮动窗口内
    };
    Q_ENUM(State)

    explicit DockWidget(const QString& unique_name, QObject* parent = nullptr);
    ~DockWidget() override;

    //! @brief 唯一名称
    QString uniqueName() const { return unique_name_; }
    //! @brief 标题
    QString title() const { return title_; }
    //! @brief 设置标题
    void setTitle(const QString& title);

    //! @brief 面板内容视图
    View* guestView() const { return guest_view_; }
    //! @brief 设置面板内容视图（非拥有）
    void setGuestView(View* guest_view);

    //! @brief 是否处于打开（停靠或浮动）状态
    bool isOpen() const { return state_ != State::Hidden; }
    //! @brief 是否浮动
    bool isFloating() const { return state_ == State::Floating; }
    //! @brief 生命周期状态
    State state() const { return state_; }

    //! @brief 是否为中央持久部件（不可拖动/浮动/关闭）
    bool isCentral() const { return central_; }
    //! @brief 标记为中央持久部件
    void setIsCentral(bool central) { central_ = central; }

    //! @brief 所属分组
    Group* group() const { return group_; }
    //! @brief 设置所属分组（由 Group::addDockWidget 维护）
    void setGroup(Group* group) { group_ = group; }

    //! @brief 打开：恢复到所属分组的原位置
    void open();
    //! @brief 关闭：隐藏但保留分组位置
    void close();
    //! @brief open() 的别名
    void show() { open(); }
    //! @brief 设为所属分组的当前选项卡
    void setAsCurrentTab();

    //! @brief 由布局层设置打开状态，不触发重排之外的副作用
    void markOpen(bool open);

Q_SIGNALS:
    //! @brief 打开状态变化
    void isOpenChanged(bool open);
    //! @brief 标题变化
    void titleChanged(const QString& title);
    //! @brief 浮动状态变化
    void isFloatingChanged(bool floating);
    //! @brief 被关闭
    void closed();

private:
    QString unique_name_;
    QString title_;
    View* guest_view_ = nullptr;
    Group* group_ = nullptr;
    State state_ = State::Hidden;
    bool central_ = false;
};

}
