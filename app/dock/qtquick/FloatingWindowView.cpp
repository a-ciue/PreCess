/**
 * @file FloatingWindowView.cpp
 * @brief 浮动窗口视图的实现
 */

#include "FloatingWindowView.h"

#include "AreaItem.h"
#include "Platform.h"
#include "core/DragController.h"
#include "core/DropArea.h"
#include "core/FloatingWindow.h"
#include "engine/SizingInfo.h"

#include <QQuickItem>

namespace dock::qtquick {

FloatingWindowView::FloatingWindowView(FloatingWindow* floating_window)
    : QQuickWindow()
    , floating_window_(floating_window)
{
    setFlags(Qt::Tool | Qt::FramelessWindowHint);
    setColor(QColor(0xf4, 0xf4, 0xf4));

    root_item_ = Platform::instance().createItemFromUrl(
        QUrl(QStringLiteral("qrc:/precess/dock/DockFloatingWindow.qml")));
    if (root_item_) {
        root_item_->setParentItem(contentItem());
        root_item_->setSize(contentItem()->size());
        root_item_->setVisible(true);
        area_host_ = root_item_->findChild<QQuickItem*>(QStringLiteral("areaHost"));
    }

    if (area_host_) {
        area_item_ = new AreaItem(area_host_);
        area_item_->setFloatingWindow(floating_window_);
        area_item_->setDropArea(floating_window_->dropArea());
    }

    if (floating_window_) {
        connect(floating_window_, &FloatingWindow::geometryChanged, this, [this](const QRect&) {
            updateFromController();
        });
        connect(floating_window_, &FloatingWindow::closed, this, [this] {
            // 核心对象即将销毁：先解除与布局区域的关联，再延迟销毁窗口视图
            if (area_item_)
                area_item_->setDropArea(nullptr);
            floating_window_ = nullptr;
            hide();
            deleteLater();
        });
        connect(floating_window_, &FloatingWindow::titleChanged, this, [this](const QString& title) {
            setTitle(title);
        });
    }

    if (contentItem()) {
        connect(contentItem(), &QQuickItem::widthChanged, this, [this] { updateAreaHost(); });
        connect(contentItem(), &QQuickItem::heightChanged, this, [this] { updateAreaHost(); });
    }

    connect(&DragController::self(), &DragController::stateChanged, this,
        [this](DragController::State state) {
            if (state == DragController::State::Idle && area_item_)
                area_item_->sync();
        });

    updateFromController();
    updateAreaHost();
}

FloatingWindowView::~FloatingWindowView() = default;

Controller* FloatingWindowView::controller() const
{
    return floating_window_;
}

void FloatingWindowView::updateFromController()
{
    if (!floating_window_)
        return;

    const QRect geometry = floating_window_->geometry();
    setPosition(geometry.topLeft());
    resize(geometry.size());

    // 首个有效几何到达后再显示，避免内容宿主尺寸为 0 时提交首帧导致空白
    if (geometry.width() > 0 && geometry.height() > 0 && !isVisible())
        show();
    update();
}

void FloatingWindowView::updateAreaHost()
{
    if (root_item_ && contentItem())
        root_item_->setSize(contentItem()->size());

    if (area_item_ && area_host_)
        area_item_->setSize(area_host_->size());

    if (floating_window_ && area_host_) {
        DropArea* drop_area = floating_window_->dropArea();
        drop_area->setGlobalOrigin(area_host_->mapToGlobal(QPointF(0, 0)).toPoint());
        drop_area->setGeometry(QRect(QPoint(0, 0), area_host_->size().toSize()));
        if (area_item_)
            area_item_->sync();
    }
    update();
}

void FloatingWindowView::setViewGeometry(const QRect& geometry)
{
    setPosition(geometry.topLeft());
    resize(geometry.size());
}

QRect FloatingWindowView::viewGeometry() const
{
    return QRect(position(), size());
}

void FloatingWindowView::setViewVisible(bool visible)
{
    setVisible(visible);
}

bool FloatingWindowView::isViewVisible() const
{
    return isVisible();
}

QSize FloatingWindowView::viewMinSize() const
{
    return QSize(0, 0);
}

QSize FloatingWindowView::viewMaxSizeHint() const
{
    return QSize(kMaxSizeLimit, kMaxSizeLimit);
}

void FloatingWindowView::raiseView()
{
    raise();
}

void FloatingWindowView::setViewCursor(Qt::CursorShape shape)
{
    Q_UNUSED(shape);
}

Qt::CursorShape FloatingWindowView::viewCursor() const
{
    return Qt::ArrowCursor;
}

QPoint FloatingWindowView::viewGlobalPosition() const
{
    return position();
}

View* FloatingWindowView::createFloatingWindowView(Controller* controller)
{
    Q_UNUSED(controller);
    return nullptr;
}

}
