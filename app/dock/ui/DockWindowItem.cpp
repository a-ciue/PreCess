/**
 * @file DockWindowItem.cpp
 * @brief 浮动窗口视图的实现
 */

#include "DockWindowItem.h"

#include "DockAreaItem.h"
#include "DockRuntime.h"
#include "docking/DragSession.h"
#include "docking/DockRegion.h"
#include "docking/DockWindow.h"
#include "tree/NodeMetrics.h"

#include <QQuickItem>

namespace dock::ui {

DockWindowItem::DockWindowItem(DockWindow* window)
    : QQuickWindow()
    , window_(window)
{
    setFlags(Qt::Tool | Qt::FramelessWindowHint);
    setColor(QColor(0xf4, 0xf4, 0xf4));

    root_item_ = DockRuntime::instance().createItemFromUrl(
        QUrl(QStringLiteral("qrc:/precess/dock/DockWindow.qml")));
    if (root_item_) {
        root_item_->setParentItem(contentItem());
        // QQuickItem::setParentItem 不改 QObject 父子关系：显式接管所有权
        root_item_->setParent(this);
        root_item_->setSize(contentItem()->size());
        root_item_->setVisible(true);
        area_host_ = root_item_->findChild<QQuickItem*>(QStringLiteral("areaHost"));
    }

    if (area_host_) {
        area_item_ = new DockAreaItem(area_host_);
        area_item_->setOwningWindow(window_);
        area_item_->setRegion(window_->region());
    }

    if (window_) {
        connect(window_, &DockWindow::geometryChanged, this, [this](const QRect&) {
            updateFromController();
        });
        connect(window_, &DockWindow::closed, this, [this] {
            // 核心对象即将销毁：先解除与布局区域的关联，再延迟销毁窗口视图
            if (area_item_)
                area_item_->setRegion(nullptr);
            window_ = nullptr;
            hide();
            deleteLater();
        });
        connect(window_, &DockWindow::titleChanged, this, [this](const QString& title) {
            setTitle(title);
        });
    }

    if (contentItem()) {
        connect(contentItem(), &QQuickItem::widthChanged, this, [this] { updateAreaHost(); });
        connect(contentItem(), &QQuickItem::heightChanged, this, [this] { updateAreaHost(); });
    }

    connect(&DragSession::self(), &DragSession::phaseChanged, this,
        [this](DragSession::Phase state) {
            if (state == DragSession::Phase::Idle && area_item_)
                area_item_->sync();
        });

    updateFromController();
    updateAreaHost();
}

DockWindowItem::~DockWindowItem() = default;

DockObject* DockWindowItem::dockObject() const
{
    return window_;
}

void DockWindowItem::updateFromController()
{
    if (!window_)
        return;

    const QRect geometry = window_->geometry();
    setPosition(geometry.topLeft());
    resize(geometry.size());

    // 首个有效几何到达后再显示，避免内容宿主尺寸为 0 时提交首帧导致空白
    if (geometry.width() > 0 && geometry.height() > 0 && !isVisible())
        show();
    update();
}

void DockWindowItem::updateAreaHost()
{
    if (root_item_ && contentItem())
        root_item_->setSize(contentItem()->size());

    if (area_item_ && area_host_)
        area_item_->setSize(area_host_->size());

    if (window_ && area_host_) {
        // region() 契约恒非空；此处防御性判空，避免核心对象异常期解引用
        DockRegion* region = window_->region();
        if (!region)
            return;
        region->setGlobalOrigin(area_host_->mapToGlobal(QPointF(0, 0)).toPoint());
        region->setGeometry(QRect(QPoint(0, 0), area_host_->size().toSize()));
        if (area_item_)
            area_item_->sync();
    }
    update();
}

void DockWindowItem::applyFrame(const QRect& geometry)
{
    setPosition(geometry.topLeft());
    resize(geometry.size());
}

QRect DockWindowItem::frame() const
{
    return QRect(position(), size());
}

void DockWindowItem::applyVisibility(bool visible)
{
    setVisible(visible);
}

bool DockWindowItem::isShown() const
{
    return isVisible();
}

QSize DockWindowItem::minExtent() const
{
    return QSize(0, 0);
}

QSize DockWindowItem::maxExtent() const
{
    return QSize(kMaxSizeLimit, kMaxSizeLimit);
}

void DockWindowItem::bringToFront()
{
    raise();
}

void DockWindowItem::setCursorShape(Qt::CursorShape shape)
{
    Q_UNUSED(shape);
}

Qt::CursorShape DockWindowItem::cursorShape() const
{
    return Qt::ArrowCursor;
}

QPoint DockWindowItem::globalOrigin() const
{
    return position();
}

DockView* DockWindowItem::createDockWindow(DockObject* controller)
{
    Q_UNUSED(controller);
    return nullptr;
}

}
