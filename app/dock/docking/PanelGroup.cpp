/**
 * @file PanelGroup.cpp
 * @brief 选项卡分组的实现
 */

#include "PanelGroup.h"

#include "DockPanel.h"
#include "DockView.h"
#include "tree/LayoutNode.h"

#include <algorithm>
#include <utility>

namespace dock {

PanelGroup::PanelGroup(QObject* parent)
    : DockObject(Kind::Group, parent)
{
}

PanelGroup::~PanelGroup()
{
    for (DockPanel* panel : std::as_const(panels_)) {
        if (panel->group() == this)
            panel->setGroup(nullptr);
    }
}

void PanelGroup::addPanel(DockPanel* panel)
{
    if (!panel || panels_.contains(panel))
        return;

    panels_.append(panel);
    panel->setGroup(this);

    connect(panel, &DockPanel::shownChanged, this, [this, panel] {
        refreshActivePanel();
        Q_EMIT panelsChanged();
        syncVisibility();
        if (panel == active_)
            Q_EMIT titleChanged(title());
    });
    connect(panel, &DockPanel::titleChanged, this, [this, panel](const QString&) {
        if (panel == active_)
            Q_EMIT titleChanged(title());
    });

    if (!active_ || !active_->isShown())
        active_ = panel;

    Q_EMIT panelsChanged();
    Q_EMIT activePanelChanged(active_);
    syncVisibility();
}

void PanelGroup::removePanel(DockPanel* panel)
{
    const int index = panels_.indexOf(panel);
    if (index < 0)
        return;

    panels_.removeAt(index);
    if (panel->group() == this)
        panel->setGroup(nullptr);
    disconnect(panel, nullptr, this, nullptr);

    if (active_ == panel)
        active_ = panels_.isEmpty() ? nullptr : panels_.first();

    Q_EMIT panelsChanged();
    Q_EMIT activePanelChanged(active_);
    syncVisibility();
}

QList<DockPanel*> PanelGroup::shownPanels() const
{
    QList<DockPanel*> result;
    for (DockPanel* panel : panels_) {
        if (panel->isShown())
            result.append(panel);
    }
    return result;
}

void PanelGroup::setActivePanel(DockPanel* panel)
{
    if (!panel || !panel->isShown() || !panels_.contains(panel))
        return;
    if (active_ == panel)
        return;

    active_ = panel;
    Q_EMIT activePanelChanged(active_);
    Q_EMIT titleChanged(title());
}

int PanelGroup::activeIndex() const
{
    return shownPanels().indexOf(active_);
}

void PanelGroup::setActiveIndex(int index)
{
    const QList<DockPanel*> shown = shownPanels();
    if (index < 0 || index >= shown.size())
        return;
    setActivePanel(shown.at(index));
}

QString PanelGroup::title() const
{
    return active_ ? active_->title() : QString();
}

void PanelGroup::syncVisibility()
{
    const bool has_shown = !shownPanels().isEmpty();
    if (node_) {
        if (node_->isVisible() != has_shown)
            node_->setVisible(has_shown);
    } else if (view()) {
        view()->applyVisibility(has_shown);
    }
}

void PanelGroup::setDetached(bool detached)
{
    for (DockPanel* panel : std::as_const(panels_))
        panel->applyDetached(detached);
}

void PanelGroup::applyGeometry(const QRect& geometry)
{
    if (view())
        view()->applyFrame(geometry);
}

QSize PanelGroup::minExtent() const
{
    QSize result(0, 0);
    const QList<DockPanel*> shown = shownPanels();
    for (DockPanel* panel : shown) {
        const DockView* content = panel->contentView();
        if (!content)
            continue;
        const QSize content_min = content->minExtent();
        result = QSize(std::max(result.width(), content_min.width()),
            std::max(result.height(), content_min.height()));
    }
    return result;
}

QSize PanelGroup::maxExtent() const
{
    QSize result(kMaxSizeLimit, kMaxSizeLimit);
    const QList<DockPanel*> shown = shownPanels();
    for (DockPanel* panel : shown) {
        const DockView* content = panel->contentView();
        if (!content)
            continue;
        const QSize content_max = content->maxExtent();
        result = QSize(std::min(result.width(), content_max.width()),
            std::min(result.height(), content_max.height()));
    }
    return result;
}

void PanelGroup::applyVisibility(bool visible)
{
    if (view())
        view()->applyVisibility(visible);
}

void PanelGroup::refreshActivePanel()
{
    if (active_ && active_->isShown() && panels_.contains(active_))
        return;

    active_ = nullptr;
    for (DockPanel* panel : std::as_const(panels_)) {
        if (panel->isShown()) {
            active_ = panel;
            break;
        }
    }
    Q_EMIT activePanelChanged(active_);
}

}
