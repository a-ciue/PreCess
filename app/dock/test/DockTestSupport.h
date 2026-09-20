/**
 * @file DockTestSupport.h
 * @brief 停靠组件测试支撑：桩视图与复刻 Main.qml 启动布局的夹具
 */

#pragma once

#include "docking/DockPanel.h"
#include "docking/DockHost.h"
#include "docking/DockView.h"
#include "tree/NodeMetrics.h"

#include <QString>

#include <memory>
#include <vector>

namespace docktest {

//! @brief 记录几何与可见性的桩视图
class StubView : public dock::DockView
{
public:
    explicit StubView(dock::DockObject* controller = nullptr, QSize min_size = QSize(50, 50))
        : dock_object_(controller)
        , min_size_(min_size)
    {
    }

    void setGlobalOrigin(const QPoint& origin) { global_origin_ = origin; }

    dock::DockObject* dockObject() const override { return dock_object_; }
    void applyFrame(const QRect& geometry) override { geometry_ = geometry; }
    QRect frame() const override { return geometry_; }
    void applyVisibility(bool visible) override { visible_ = visible; }
    bool isShown() const override { return visible_; }
    QSize minExtent() const override { return min_size_; }
    QSize maxExtent() const override { return QSize(dock::kMaxSizeLimit, dock::kMaxSizeLimit); }
    void bringToFront() override { }
    QPoint globalOrigin() const override { return global_origin_ + geometry_.topLeft(); }
    dock::DockView* createDockWindow(dock::DockObject* controller) override
    {
        auto view = std::make_unique<StubView>(controller);
        dock::DockView* raw = view.get();
        floating_views.push_back(std::move(view));
        return raw;
    }

    std::vector<std::unique_ptr<StubView>> floating_views;

private:
    dock::DockObject* dock_object_ = nullptr;
    QSize min_size_;
    QRect geometry_;
    QPoint global_origin_;
    bool visible_ = true;
};

//! @brief 复刻 Main.qml 启动布局的测试夹具
struct DockFixture {
    DockFixture()
        : host { QStringLiteral("PreCessMainLayout") }
        , central_view(&host)
    {
        host.setView(&main_view);
        host.setCentralContentView(&central_view);
        host.setFrame(QRect(0, 0, 1600, 900));
    }

    //! @brief 创建带桩视图的面板
    dock::DockPanel* makeDock(const QString& name, const QString& title,
        QSize view_min = QSize(50, 50))
    {
        auto* panel = new dock::DockPanel(name, &host);
        panel->setTitle(title);
        auto view = std::make_unique<StubView>(panel, view_min);
        panel->setContentView(view.get());
        views.push_back(std::move(view));
        return panel;
    }

    dock::DockHost host;
    StubView main_view { &host };
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
        dock::DockHost* window = &f.host;
        window->placePanel(object_tree, dock::DockEdge::Left, nullptr, QSize(250, 0));
        window->placePanel(side_bar, dock::DockEdge::Bottom, object_tree, QSize(0, 400));
        window->placePanel(attribute_render, dock::DockEdge::Bottom, object_tree, QSize(0, 300),
            dock::PanelLaunch::Hidden);
        window->placePanel(console, dock::DockEdge::Bottom, nullptr, QSize(0, 300),
            dock::PanelLaunch::Hidden);
        window->placePanel(python_console, dock::DockEdge::Right, nullptr, QSize(450, 0),
            dock::PanelLaunch::Hidden);
        window->placePanel(output_log, dock::DockEdge::Bottom, nullptr, QSize(0, 300),
            dock::PanelLaunch::Hidden);
        window->placePanel(preferences, dock::DockEdge::Top, object_tree, QSize(0, 200),
            dock::PanelLaunch::Hidden);
    }

    dock::DockPanel* object_tree;
    dock::DockPanel* side_bar;
    dock::DockPanel* attribute_render;
    dock::DockPanel* console;
    dock::DockPanel* python_console;
    dock::DockPanel* output_log;
    dock::DockPanel* preferences;
};

}
