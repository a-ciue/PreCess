/**
 * @file DockPanel.cpp
 * @brief 逻辑停靠面板的实现
 */

#include "DockPanel.h"

#include "DockCatalog.h"
#include "PanelGroup.h"

namespace dock {

DockPanel::DockPanel(const QString& unique_name, QObject* parent)
    : DockObject(Kind::Panel, parent)
    , unique_name_(unique_name)
    , title_(unique_name)
{
    DockCatalog::self().registerPanel(this);
}

DockPanel::~DockPanel()
{
    DockCatalog::self().unregisterPanel(this);
}

void DockPanel::setTitle(const QString& title)
{
    if (title_ == title)
        return;

    title_ = title;
    Q_EMIT titleChanged(title_);
}

void DockPanel::setContentView(DockView* content_view)
{
    content_view_ = content_view;
}

void DockPanel::setFeature(Feature feature, bool on)
{
    const Features updated = on ? (features_ | feature) : (features_ & ~Features(feature));
    if (updated == features_)
        return;

    features_ = updated;
    Q_EMIT featuresChanged(features_);
}

void DockPanel::showPanel()
{
    applyShown(true);
}

void DockPanel::hidePanel()
{
    applyShown(false);
}

void DockPanel::applyShown(bool shown)
{
    const Lifecycle target = shown ? Lifecycle::Docked : Lifecycle::Hidden;
    if (lifecycle_ == target)
        return;

    if (lifecycle_ == Lifecycle::Detached && shown)
        return; // 独立窗口状态下的显示由其窗口置顶处理

    lifecycle_ = target;
    if (group_)
        group_->syncVisibility();

    Q_EMIT shownChanged(shown);
    if (!shown)
        Q_EMIT hidden();
}

void DockPanel::applyDetached(bool detached)
{
    if (detached) {
        if (lifecycle_ == Lifecycle::Detached)
            return;
        lifecycle_ = Lifecycle::Detached;
        Q_EMIT detachedChanged(true);
    } else {
        if (lifecycle_ != Lifecycle::Detached)
            return;
        lifecycle_ = Lifecycle::Docked;
        Q_EMIT detachedChanged(false);
    }
}

}
