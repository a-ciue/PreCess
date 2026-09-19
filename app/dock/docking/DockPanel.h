/**
 * @file DockPanel.h
 * @brief 逻辑停靠面板：可显隐、可停靠、可拆出为独立窗口
 */

#pragma once

#include "DockObject.h"

#include <QString>

namespace dock {

class PanelGroup;
class DockView;

/**
 * @brief 逻辑停靠面板
 *
 * 面板内容由视图层提供的 DockView 承载。生命周期：
 * Hidden（不占布局空间，可回到所属分组的原位置）/ Docked / Detached。
 * hidePanel() 不销毁面板；showPanel() 恢复显示。
 */
class DockPanel : public DockObject
{
    Q_OBJECT
public:
    //! @brief 生命周期
    enum class Lifecycle {
        Hidden, //!< 已隐藏
        Docked, //!< 停靠在布局中
        Detached //!< 在独立窗口中
    };
    Q_ENUM(Lifecycle)

    explicit DockPanel(const QString& unique_name, QObject* parent = nullptr);
    ~DockPanel() override;

    //! @brief 唯一名称
    QString uniqueName() const { return unique_name_; }
    //! @brief 标题
    QString title() const { return title_; }
    //! @brief 设置标题
    void setTitle(const QString& title);

    //! @brief 面板内容视图
    DockView* contentView() const { return content_view_; }
    //! @brief 设置面板内容视图（非拥有）
    void setContentView(DockView* content_view);

    //! @brief 是否处于显示（停靠或独立窗口）状态
    bool isShown() const { return lifecycle_ != Lifecycle::Hidden; }
    //! @brief 是否在独立窗口中
    bool isDetached() const { return lifecycle_ == Lifecycle::Detached; }
    //! @brief 生命周期
    Lifecycle lifecycle() const { return lifecycle_; }

    //! @brief 是否为中央持久部件（不可拖动/拆出/隐藏）
    bool isCentral() const { return central_; }
    //! @brief 标记为中央持久部件
    void setIsCentral(bool central) { central_ = central; }

    //! @brief 所属分组
    PanelGroup* group() const { return group_; }
    //! @brief 设置所属分组（由 PanelGroup::addPanel 维护）
    void setGroup(PanelGroup* group) { group_ = group; }

    //! @brief 显示：恢复到所属分组的原位置
    void showPanel();
    //! @brief 隐藏但保留分组位置
    void hidePanel();
    //! @brief 设为所属分组的当前选项卡
    void activateTab();

    //! @brief 由布局层同步显示状态
    void applyShown(bool shown);
    //! @brief 由布局层同步拆出状态
    void applyDetached(bool detached);

Q_SIGNALS:
    //! @brief 显示状态变化
    void shownChanged(bool shown);
    //! @brief 标题变化
    void titleChanged(const QString& title);
    //! @brief 拆出状态变化
    void detachedChanged(bool detached);
    //! @brief 被隐藏
    void hidden();

private:
    QString unique_name_;
    QString title_;
    DockView* content_view_ = nullptr;
    PanelGroup* group_ = nullptr;
    Lifecycle lifecycle_ = Lifecycle::Hidden;
    bool central_ = false;
};

}
