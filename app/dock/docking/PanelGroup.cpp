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
    : DockObject(parent)
{
}

PanelGroup::~PanelGroup()
{
    // 防御：任何遗漏路径下，节点不得保留指向本分组的悬空 client
    if (node_)
        node_->setClient(nullptr);

    for (DockPanel* panel : std::as_const(panels_)) {
        if (panel->group() == this)
            panel->setGroup(nullptr);
    }
}

void PanelGroup::addPanel(DockPanel* panel)
{
    insertPanelAt(panel, panels_.size());
}

void PanelGroup::insertPanel(DockPanel* panel, int shown_index)
{
    if (!panel)
        return;

    if (panels_.contains(panel)) {
        const int from = shownPanels().indexOf(panel);
        if (from >= 0 && shown_index >= 0)
            movePanel(from, shown_index);
        return;
    }

    // 显示面板下标映射到 panels_ 下标：插到目标显示面板之前，越界追加
    const QList<DockPanel*> shown = shownPanels();
    int panels_index = panels_.size();
    if (shown_index >= 0 && shown_index < shown.size())
        panels_index = panels_.indexOf(shown.at(shown_index));
    insertPanelAt(panel, panels_index);
}

bool PanelGroup::movePanel(int from, int to)
{
    const QList<DockPanel*> shown = shownPanels();
    if (from < 0 || from >= shown.size() || to < 0 || to > shown.size())
        return false;

    // to 为"插到第 to 个显示面板之前"；移动后的最终显示下标
    const int final_index = to > from ? to - 1 : to;
    if (final_index == from)
        return false;

    DockPanel* panel = shown.at(from);
    const int panels_from = panels_.indexOf(panel);
    if (panels_from < 0)
        return false;

    QList<DockPanel*> target_shown = shown;
    target_shown.removeAt(from);
    int panels_to = panels_.size();
    if (final_index < target_shown.size())
        panels_to = panels_.indexOf(target_shown.at(final_index));

    panels_.takeAt(panels_from);
    if (panels_to > panels_from)
        --panels_to; // 取出后其后元素前移
    panels_.insert(panels_to, panel);

    Q_EMIT panelsChanged();
    return true;
}

void PanelGroup::insertPanelAt(DockPanel* panel, int panels_index)
{
    if (!panel || panels_.contains(panel))
        return;

    panels_index = std::clamp(panels_index, 0, static_cast<int>(panels_.size()));
    panels_.insert(panels_index, panel);
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
    connect(panel, &DockPanel::featuresChanged, this, [this](DockPanel::Features) {
        Q_EMIT panelsChanged();
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

    Q_EMIT panelsChanged();

    // 移除激活面板时交给统一的补位逻辑（跳过隐藏面板）；
    // 移除其他面板不改变激活态，无需重复通知
    if (active_ == panel) {
        active_ = nullptr;
        refreshActivePanel();
    }
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

void PanelGroup::hideOthers(DockPanel* panel)
{
    if (!panel || !panels_.contains(panel))
        return;

    const QList<DockPanel*> shown = shownPanels();
    for (DockPanel* other : shown) {
        if (other != panel)
            other->hidePanel();
    }
}

DockPanel::Features PanelGroup::features() const
{
    // 中央持久分组不提供任何可操作能力
    if (central_)
        return DockPanel::Feature::NoFeature;

    DockPanel::Features result = DockPanel::Feature::DefaultFeatures;
    const QList<DockPanel*> shown = shownPanels();
    for (DockPanel* panel : shown)
        result &= panel->features();
    return result;
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
