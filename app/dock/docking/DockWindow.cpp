/**
 * @file DockWindow.cpp
 * @brief 浮动窗口的实现
 */

#include "DockWindow.h"

#include "DockCatalog.h"
#include "DockRegion.h"
#include "PanelGroup.h"
#include "tree/LayoutNode.h"

namespace dock {

DockWindow::DockWindow(QObject* parent)
    : DockObject(parent)
    , region_(new DockRegion(this))
{
    DockCatalog::self().registerWindow(this);
}

DockWindow::~DockWindow()
{
    DockCatalog::self().unregisterWindow(this);
}

void DockWindow::takeGroup(PanelGroup* group, LayoutNode* item)
{
#ifdef QT_DEBUG
    Q_ASSERT_X(!group_ || group_ == group, "DockWindow",
        "takeGroup on a window that already hosts another group");
#endif
    group_ = group;
    if (!group_) {
        region_->setRootNode(nullptr);
        Q_EMIT groupChanged();
        return;
    }

    region_->setRootNode(item);
    if (item) {
        item->setId(group_->title());
        group_->setNode(item);
    }
    group_->setDetached(true);
    Q_EMIT titleChanged(title());
    Q_EMIT groupChanged();
}

void DockWindow::adoptTree(PanelGroup* primary, LayoutNode* root)
{
    group_ = primary;
    region_->setRootNode(root);
    if (primary)
        primary->setDetached(true);
    Q_EMIT titleChanged(title());
    Q_EMIT groupChanged();
}

LayoutNode* DockWindow::releaseGroup()
{
    if (!group_)
        return nullptr;

    PanelGroup* released = group_;
    // 仅当分组就是区域根（独占浮窗）时整树摘出；多分组浮窗只摘叶子
    LayoutNode* node = nullptr;
    if (region_->rootNode() == released->node())
        node = region_->takeRootNode();
    else if (region_->extractGroupNode(released))
        node = released->node();

    released->setDetached(false);
    // 解除窗口所有权：归还出的分组不再随本窗口销毁而被连带删除
    if (released->parent() == this)
        released->setParent(nullptr);
    group_ = nullptr;
    Q_EMIT groupChanged();
    Q_EMIT titleChanged(title());
    return node;
}

void DockWindow::setGeometry(const QRect& geometry)
{
    if (geometry_ == geometry)
        return;

    geometry_ = geometry;
    region_->setGlobalOrigin(geometry.topLeft());
    region_->setGeometry(QRect(QPoint(0, 0), geometry.size()));
    Q_EMIT geometryChanged(geometry_);
}

QString DockWindow::title() const
{
    return group_ ? group_->title() : QString();
}

void DockWindow::close()
{
    Q_EMIT closed();
}

}
