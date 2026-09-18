/**
 * @file MainWindow.cpp
 * @brief 主停靠窗口的实现
 */

#include "MainWindow.h"

#include "DockRegistry.h"
#include "DockWidget.h"
#include "DropArea.h"
#include "Group.h"
#include "engine/Item.h"

namespace dock {

MainWindow::MainWindow(const QString& unique_name, MainWindowOptions options, QObject* parent)
    : Controller(Type::MainWindow, parent)
    , unique_name_(unique_name)
    , options_(options)
    , drop_area_(new DropArea(this))
{
    if (options_ & MainWindowOption_HasCentralWidget) {
        central_dock_widget_ = new DockWidget(unique_name_ + QStringLiteral("-CentralDockWidget"), this);
        central_dock_widget_->setIsCentral(true);

        central_group_ = new Group(this);
        central_group_->setIsCentral(true);
        central_group_->addDockWidget(central_dock_widget_);

        central_item_ = new Item(central_group_);
        central_item_->setId(central_dock_widget_->uniqueName());
        central_group_->setLayoutItem(central_item_);
        drop_area_->setCentralItem(central_item_);
        drop_area_->setRootItem(central_item_);
        central_dock_widget_->markOpen(true);
    }

    DockRegistry::self().setMainWindow(this);
}

MainWindow::~MainWindow()
{
    if (DockRegistry::self().mainWindow() == this)
        DockRegistry::self().setMainWindow(nullptr);
}

void MainWindow::setCentralGuestView(View* guest_view)
{
    if (central_dock_widget_)
        central_dock_widget_->setGuestView(guest_view);
}

void MainWindow::addDockWidget(DockWidget* dock_widget, Location location, DockWidget* relative_to,
    const QSize& preferred_size, InitialVisibilityOption option)
{
    drop_area_->addDockWidget(dock_widget, location, relative_to, preferred_size, option);
}

void MainWindow::addDockWidgetAsTab(DockWidget* dock_widget)
{
    if (central_group_)
        drop_area_->addDockWidgetAsTab(dock_widget, central_group_);
}

void MainWindow::setAreaGeometry(const QRect& geometry)
{
    drop_area_->setGeometry(geometry);
    Q_EMIT areaGeometryChanged(geometry);
}

}
