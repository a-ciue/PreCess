/**
 * @file DockingAreaView.cpp
 * @brief 主停靠区域视图的实现
 */

#include "DockingAreaView.h"

#include "AreaItem.h"
#include "DockWidgetInstantiator.h"
#include "FloatingWindowView.h"
#include "GroupView.h"
#include "GuestView.h"
#include "Platform.h"
#include "core/DockWidget.h"
#include "core/DropArea.h"
#include "core/FloatingWindow.h"
#include "core/Group.h"
#include "core/MainWindow.h"
#include "engine/Item.h"

#include <QQuickWindow>

namespace dock::qtquick {

DockingAreaView::DockingAreaView(QQuickItem* parent)
    : QQuickItem(parent)
{
}

DockingAreaView::~DockingAreaView() = default;

void DockingAreaView::setUniqueName(const QString& unique_name)
{
    if (unique_name_ == unique_name)
        return;
    unique_name_ = unique_name;
    Q_EMIT uniqueNameChanged();
}

void DockingAreaView::setOptions(int options)
{
    if (options_ == options)
        return;
    options_ = options;
    Q_EMIT optionsChanged();
}

void DockingAreaView::setPersistentCentralItemFileName(const QUrl& file)
{
    if (central_file_ == file)
        return;
    central_file_ = file;
    Q_EMIT persistentCentralItemFileNameChanged();
}

void DockingAreaView::componentComplete()
{
    QQuickItem::componentComplete();

    if (unique_name_.isEmpty())
        unique_name_ = QStringLiteral("DockingArea");

    main_window_ = new dock::MainWindow(unique_name_, MainWindowOptions(options_), this);
    main_window_->setView(this);

    area_item_ = new AreaItem(this);
    area_item_->setSize(size());
    area_item_->setDropArea(main_window_->dropArea());

    watchWindow();
    loadCentralItem();
    updateAreaGeometry();
}

void DockingAreaView::geometryChange(const QRectF& new_geometry, const QRectF& old_geometry)
{
    QQuickItem::geometryChange(new_geometry, old_geometry);
    if (area_item_)
        area_item_->setSize(new_geometry.size());
    updateAreaGeometry();
}

void DockingAreaView::updateAreaGeometry()
{
    if (!main_window_)
        return;

    const QPoint origin = mapToGlobal(QPointF(0, 0)).toPoint();
    main_window_->dropArea()->setGlobalOrigin(origin);
    main_window_->setAreaGeometry(QRect(QPoint(0, 0), size().toSize()));
    if (area_item_)
        area_item_->sync();
}

void DockingAreaView::watchWindow()
{
    if (window_watched_)
        return;

    QQuickWindow* host = window();
    if (!host)
        return;

    connect(host, &QWindow::xChanged, this, [this] { updateAreaGeometry(); });
    connect(host, &QWindow::yChanged, this, [this] { updateAreaGeometry(); });
    window_watched_ = true;
}

void DockingAreaView::loadCentralItem()
{
    if (central_file_.isEmpty() || !main_window_ || !main_window_->centralDockWidget())
        return;

    QQuickItem* item = Platform::instance().createItemFromUrl(central_file_);
    if (!item)
        return;

    Platform::instance().registerGuestItem(main_window_->centralDockWidget(), item);
    central_view_ = std::make_unique<GuestView>(main_window_->centralDockWidget(), item);
    main_window_->setCentralGuestView(central_view_.get());

    // 中央分组视图可能先于 guest 注册创建：立即刷新一次完成挂载
    if (dock::Group* central_group = main_window_->centralGroup()) {
        if (GroupView* view = Platform::instance().groupView(central_group))
            view->syncFromGroup();
    }
}

void DockingAreaView::addDockWidget(QQuickItem* dock_widget, int location, QQuickItem* relative_to,
    QSize preferred_size, int option)
{
    auto* instantiator = qobject_cast<DockWidgetInstantiator*>(dock_widget);
    if (!main_window_ || !instantiator || !instantiator->dockWidget())
        return;

    auto* relative_instantiator = qobject_cast<DockWidgetInstantiator*>(relative_to);
    dock::DockWidget* dock = instantiator->dockWidget();

    main_window_->addDockWidget(dock, static_cast<Location>(location),
        relative_instantiator ? relative_instantiator->dockWidget() : nullptr, preferred_size,
        static_cast<InitialVisibilityOption>(option));

    connect(dock, &dock::DockWidget::isOpenChanged, this, [this](bool) {
        if (area_item_)
            area_item_->sync();
    });
    connect(dock, &dock::DockWidget::isFloatingChanged, this, [this](bool) {
        if (area_item_)
            area_item_->sync();
    });

    if (area_item_)
        area_item_->sync();
}

void DockingAreaView::addDockWidgetAsTab(QQuickItem* dock_widget)
{
    auto* instantiator = qobject_cast<DockWidgetInstantiator*>(dock_widget);
    if (!main_window_ || !instantiator || !instantiator->dockWidget())
        return;

    main_window_->addDockWidgetAsTab(instantiator->dockWidget());
    if (area_item_)
        area_item_->sync();
}

Controller* DockingAreaView::controller() const
{
    return main_window_;
}

void DockingAreaView::setViewGeometry(const QRect& geometry)
{
    setPosition(geometry.topLeft());
    setSize(geometry.size());
}

QRect DockingAreaView::viewGeometry() const
{
    return QRect(position().toPoint(), size().toSize());
}

void DockingAreaView::setViewVisible(bool visible)
{
    setVisible(visible);
}

bool DockingAreaView::isViewVisible() const
{
    return isVisible();
}

QSize DockingAreaView::viewMinSize() const
{
    return QSize(0, 0);
}

QSize DockingAreaView::viewMaxSizeHint() const
{
    return QSize(kMaxSizeLimit, kMaxSizeLimit);
}

void DockingAreaView::raiseView()
{
    setZ(z() + 1);
}

void DockingAreaView::setViewCursor(Qt::CursorShape shape)
{
    setCursor(shape);
}

Qt::CursorShape DockingAreaView::viewCursor() const
{
    return cursor().shape();
}

QPoint DockingAreaView::viewGlobalPosition() const
{
    return mapToGlobal(QPointF(0, 0)).toPoint();
}

View* DockingAreaView::createFloatingWindowView(Controller* controller)
{
    auto* floating_window = qobject_cast<dock::FloatingWindow*>(controller);
    if (!floating_window)
        return nullptr;

    auto* view = new FloatingWindowView(floating_window);
    // 窗口在收到首个有效几何后自行显示（见 FloatingWindowView::updateFromController）
    return view;
}

}
