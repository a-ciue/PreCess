/**
 * @file DockPanel.h
 * @brief 逻辑停靠面板：可显隐、可停靠、可拆出为独立窗口
 */

#pragma once

#include "DockObject.h"

#include <QFlags>
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

    //! @brief 面板能力位（UI 按位门控拖拽/浮动/关闭入口）
    enum Feature {
        NoFeature = 0x00, //!< 无能力
        Closable = 0x01, //!< 可关闭
        Movable = 0x02, //!< 可拖动（含组内重排与跨区域移动）
        Floatable = 0x04, //!< 可浮动为独立窗口
        DefaultFeatures = Closable | Movable | Floatable
    };
    Q_DECLARE_FLAGS(Features, Feature)
    Q_FLAG(Features)

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

    //! @brief 是否为中央持久部件（不可拖动/拆出/隐藏）
    bool isCentral() const { return central_; }
    //! @brief 标记为中央持久部件
    void setIsCentral(bool central) { central_ = central; }

    //! @brief 面板能力位
    Features features() const { return features_; }
    //! @brief 开关单项能力
    void setFeature(Feature feature, bool on = true);
    //! @brief 是否具备某项能力
    bool hasFeature(Feature feature) const { return features_.testFlag(feature); }

    //! @brief 所属分组
    PanelGroup* group() const { return group_; }
    //! @brief 设置所属分组（由 PanelGroup::addPanel 维护）
    void setGroup(PanelGroup* group) { group_ = group; }

    //! @brief 显示：恢复到所属分组的原位置
    void showPanel();
    //! @brief 隐藏但保留分组位置
    void hidePanel();

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
    //! @brief 能力位变化
    void featuresChanged(DockPanel::Features features);
    //! @brief 被隐藏
    void hidden();

private:
    QString unique_name_;
    QString title_;
    DockView* content_view_ = nullptr;
    PanelGroup* group_ = nullptr;
    Lifecycle lifecycle_ = Lifecycle::Hidden;
    bool central_ = false;
    Features features_ = Feature::DefaultFeatures;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(DockPanel::Features)

}
