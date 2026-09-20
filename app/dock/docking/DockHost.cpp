/**
 * @file DockHost.cpp
 * @brief 主停靠窗口的实现
 */

#include "DockHost.h"

#include "DockCatalog.h"
#include "DockPanel.h"
#include "DockRegion.h"
#include "PanelGroup.h"
#include "tree/LayoutNode.h"

namespace dock {

DockHost::DockHost(const QString& unique_name, QObject* parent)
    : DockObject(Kind::Host, parent)
    , unique_name_(unique_name)
    , region_(new DockRegion(this))
{
    central_panel_ = new DockPanel(unique_name_ + QStringLiteral("-CentralPanel"), this);
    central_panel_->setIsCentral(true);

    central_group_ = new PanelGroup(this);
    central_group_->setIsCentral(true);
    central_group_->addPanel(central_panel_);

    central_node_ = new LayoutNode(central_group_);
    central_node_->setId(central_panel_->uniqueName());
    central_group_->setNode(central_node_);
    region_->setCentralNode(central_node_);
    region_->setRootNode(central_node_);
    central_panel_->applyShown(true);

    DockCatalog::self().setHost(this);
}

DockHost::~DockHost()
{
    if (DockCatalog::self().host() == this)
        DockCatalog::self().setHost(nullptr);
}

void DockHost::setCentralContentView(DockView* content_view)
{
    central_panel_->setContentView(content_view);
}

void DockHost::placePanel(DockPanel* panel, DockEdge edge, DockPanel* relative_to,
    const QSize& preferred_size, PanelLaunch launch)
{
    region_->placePanel(panel, edge, relative_to, preferred_size, launch);
}

void DockHost::stackPanel(DockPanel* panel)
{
    region_->stackPanel(panel, central_group_);
}

void DockHost::hideOtherGroups(PanelGroup* except)
{
    const QList<DockPanel*> panels = DockCatalog::self().panels();
    for (DockPanel* panel : panels) {
        if (!panel || panel == central_panel_ || !panel->isShown())
            continue;
        if (except && panel->group() == except)
            continue;
        panel->hidePanel();
    }
}

void DockHost::setFrame(const QRect& frame)
{
    region_->setGeometry(frame);
    Q_EMIT frameChanged(frame);
}

}
