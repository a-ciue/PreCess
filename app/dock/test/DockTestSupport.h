/**
 * @file DockTestSupport.h
 * @brief 停靠组件测试支撑：桩视图与复刻 Main.qml 启动布局的夹具
 */

#pragma once

#include "core/DockWidget.h"
#include "core/MainWindow.h"
#include "core/View.h"
#include "engine/SizingInfo.h"

#include <QString>

#include <memory>
#include <vector>

namespace docktest {

//! @brief 记录几何与可见性的桩视图
class StubView : public dock::View
{
public:
    explicit StubView(dock::Controller* controller = nullptr, QSize min_size = QSize(50, 50))
        : controller_(controller)
        , min_size_(min_size)
    {
    }

    void setGlobalOrigin(const QPoint& origin) { global_origin_ = origin; }

    dock::Controller* controller() const override { return controller_; }
    void setViewGeometry(const QRect& geometry) override { geometry_ = geometry; }
    QRect viewGeometry() const override { return geometry_; }
    void setViewVisible(bool visible) override { visible_ = visible; }
    bool isViewVisible() const override { return visible_; }
    QSize viewMinSize() const override { return min_size_; }
    QSize viewMaxSizeHint() const override { return QSize(dock::kMaxSizeLimit, dock::kMaxSizeLimit); }
    void setParentView(dock::View* parent) override { parent_ = parent; }
    dock::View* parentView() const override { return parent_; }
    void raiseView() override { }
    void setViewCursor(Qt::CursorShape shape) override { cursor_ = shape; }
    Qt::CursorShape viewCursor() const override { return cursor_; }
    QPoint viewGlobalPosition() const override { return global_origin_ + geometry_.topLeft(); }
    dock::View* createFloatingWindowView(dock::Controller* controller) override
    {
        auto view = std::make_unique<StubView>(controller);
        dock::View* raw = view.get();
        floating_views.push_back(std::move(view));
        return raw;
    }

    std::vector<std::unique_ptr<StubView>> floating_views;

private:
    dock::Controller* controller_ = nullptr;
    QSize min_size_;
    QRect geometry_;
    QPoint global_origin_;
    bool visible_ = true;
    dock::View* parent_ = nullptr;
    Qt::CursorShape cursor_ = Qt::ArrowCursor;
};

//! @brief 复刻 Main.qml 启动布局的测试夹具
struct DockFixture {
    DockFixture()
        : main_window(QStringLiteral("PreCessMainLayout"), dock::MainWindowOption_HasCentralWidget)
        , central_view(&main_window)
    {
        main_window.setView(&main_view);
        main_window.setCentralGuestView(&central_view);
        main_window.setAreaGeometry(QRect(0, 0, 1600, 900));
    }

    //! @brief 创建带桩视图的面板
    dock::DockWidget* makeDock(const QString& name, const QString& title,
        QSize view_min = QSize(50, 50))
    {
        auto* dock_widget = new dock::DockWidget(name, &main_window);
        dock_widget->setTitle(title);
        auto view = std::make_unique<StubView>(dock_widget, view_min);
        dock_widget->setGuestView(view.get());
        views.push_back(std::move(view));
        return dock_widget;
    }

    dock::MainWindow main_window;
    StubView main_view { &main_window };
    StubView central_view;
    std::vector<std::unique_ptr<StubView>> views;
};

//! @brief 与 Main.qml Component.onCompleted 等价的停靠调用序列
struct PlacedDocks {
    PlacedDocks(DockFixture& f)
        : object_tree(f.makeDock(QStringLiteral("objectTree"), QStringLiteral("对象树")))
        , side_bar(f.makeDock(QStringLiteral("sideBar"), QStringLiteral("属性列表")))
        , attribute_render(f.makeDock(QStringLiteral("attributeRender"), QStringLiteral("属性渲染")))
        , console(f.makeDock(QStringLiteral("console"), QStringLiteral("JavaScript 控制台")))
        , python_console(f.makeDock(QStringLiteral("pythonConsole"), QStringLiteral("Python 控制台")))
        , output_log(f.makeDock(QStringLiteral("outputLog"), QStringLiteral("日志")))
        , preferences(f.makeDock(QStringLiteral("preferences"), QStringLiteral("偏好设置")))
    {
        dock::MainWindow* window = &f.main_window;
        window->addDockWidget(object_tree, dock::Location_OnLeft, nullptr, QSize(250, 0));
        window->addDockWidget(side_bar, dock::Location_OnBottom, object_tree, QSize(0, 400));
        window->addDockWidget(attribute_render, dock::Location_OnBottom, object_tree, QSize(0, 300),
            dock::StartHidden);
        window->addDockWidget(console, dock::Location_OnBottom, nullptr, QSize(0, 300),
            dock::StartHidden);
        window->addDockWidget(python_console, dock::Location_OnRight, nullptr, QSize(450, 0),
            dock::StartHidden);
        window->addDockWidget(output_log, dock::Location_OnBottom, nullptr, QSize(0, 300),
            dock::StartHidden);
        window->addDockWidget(preferences, dock::Location_OnTop, object_tree, QSize(0, 200),
            dock::StartHidden);
    }

    dock::DockWidget* object_tree;
    dock::DockWidget* side_bar;
    dock::DockWidget* attribute_render;
    dock::DockWidget* console;
    dock::DockWidget* python_console;
    dock::DockWidget* output_log;
    dock::DockWidget* preferences;
};

}
