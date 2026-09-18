/**
 * @file TestDockCore.cpp
 * @brief 停靠核心语义单元测试：命令式停靠布局、关闭/恢复、中央部件、选项卡
 */

#include "core/DockRegistry.h"
#include "core/DockWidget.h"
#include "core/DropArea.h"
#include "core/Group.h"
#include "core/MainWindow.h"
#include "core/View.h"
#include "engine/Item.h"
#include "engine/ItemBoxContainer.h"
#include "engine/SizingInfo.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>

namespace {

//! @brief 记录几何与可见性的桩视图
class StubView : public dock::View
{
public:
    explicit StubView(dock::Controller* controller, QSize min_size = QSize(50, 50))
        : controller_(controller)
        , min_size_(min_size)
    {
    }

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
    QPoint viewGlobalPosition() const override { return QPoint(0, 0); }
    dock::View* createFloatingWindowView(dock::Controller*) override { return nullptr; }

private:
    dock::Controller* controller_;
    QSize min_size_;
    QRect geometry_;
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
        docks.push_back(dock_widget);
        return dock_widget;
    }

    dock::MainWindow main_window;
    StubView central_view;
    std::vector<std::unique_ptr<StubView>> views;
    std::vector<dock::DockWidget*> docks;
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

TEST_CASE("DockCore: initial layout matches Main.qml placement")
{
    DockFixture f;
    PlacedDocks placed(f);

    dock::DockWidget* central = f.main_window.centralDockWidget();
    REQUIRE(central != nullptr);
    REQUIRE(central->isOpen());

    // 对象树固定在左侧 250px，纵向与属性列表均分
    const dock::Item* object_tree_item = placed.object_tree->group()->layoutItem();
    REQUIRE(object_tree_item != nullptr);
    CHECK(object_tree_item->isVisible());
    CHECK(object_tree_item->geometry().x() == 0);
    CHECK(object_tree_item->geometry().width() == 250);
    CHECK(object_tree_item->geometry().height() > 0);
    CHECK(object_tree_item->geometry().height() < 900);

    // 中央部件占据其余宽度，且是受保护节点
    const dock::Item* central_item = central->group()->layoutItem();
    REQUIRE(central_item != nullptr);
    CHECK(f.main_window.dropArea()->isCentralItem(central_item));
    CHECK(central_item->geometry().x() == 250 + dock::kSeparatorThickness);
    CHECK(central_item->geometry().width() == 1600 - 250 - dock::kSeparatorThickness);
    CHECK(central_item->geometry().height() == 900);

    // 属性列表停靠在对象树下方，高度约 400
    const dock::Item* side_bar_item = placed.side_bar->group()->layoutItem();
    REQUIRE(side_bar_item != nullptr);
    CHECK(side_bar_item->isVisible());
    CHECK(side_bar_item->geometry().width() == 250);
    const int side_bar_height = side_bar_item->geometry().height();
    CHECK(side_bar_height >= 398);
    CHECK(side_bar_height <= 403);
    CHECK(object_tree_item->geometry().height() + dock::kSeparatorThickness + side_bar_height == 900);
}

TEST_CASE("DockCore: StartHidden docks stay closed until opened")
{
    DockFixture f;
    PlacedDocks placed(f);

    const dock::DockWidget* hidden_docks[] = { placed.attribute_render, placed.console,
        placed.python_console, placed.output_log, placed.preferences };
    for (const dock::DockWidget* dock_widget : hidden_docks) {
        CHECK_FALSE(dock_widget->isOpen());
        REQUIRE(dock_widget->group() != nullptr);
        CHECK_FALSE(dock_widget->group()->layoutItem()->isVisible());
    }
}

TEST_CASE("DockCore: open shows hidden dock at remembered position")
{
    DockFixture f;
    PlacedDocks placed(f);

    const dock::Item* object_tree_item = placed.object_tree->group()->layoutItem();
    const dock::Item* side_bar_item = placed.side_bar->group()->layoutItem();
    const int side_bar_height = side_bar_item->geometry().height();

    // 打开底部日志：占据布局底部约 300px
    placed.console->open();
    CHECK(placed.console->isOpen());
    const dock::Item* console_item = placed.console->group()->layoutItem();
    REQUIRE(console_item != nullptr);
    CHECK(console_item->isVisible());
    CHECK(console_item->geometry().width() == 1600);
    const int console_height = console_item->geometry().height();
    CHECK(console_height >= 298);
    CHECK(console_height <= 303);

    // 左侧区域高度收缩
    const int left_total = object_tree_item->geometry().height() + dock::kSeparatorThickness
        + side_bar_item->geometry().height();
    const int expected_left_total = 900 - dock::kSeparatorThickness - console_height;
    const int left_difference = left_total - expected_left_total;
    CHECK(left_difference >= -2);
    CHECK(left_difference <= 2);

    // 再次关闭后恢复原状（占比换算取整允许 1~2px 漂移）
    placed.console->close();
    CHECK_FALSE(placed.console->isOpen());
    CHECK_FALSE(console_item->isVisible());
    const int restored_total = object_tree_item->geometry().height()
        + dock::kSeparatorThickness + side_bar_height;
    CHECK(restored_total >= 898);
    CHECK(restored_total <= 900);
}

TEST_CASE("DockCore: closing and reopening a docked panel restores its size")
{
    DockFixture f;
    PlacedDocks placed(f);

    const dock::Item* object_tree_item = placed.object_tree->group()->layoutItem();
    const dock::Item* side_bar_item = placed.side_bar->group()->layoutItem();
    const int side_bar_height_before = side_bar_item->geometry().height();

    placed.object_tree->close();
    CHECK_FALSE(placed.object_tree->isOpen());
    CHECK_FALSE(object_tree_item->isVisible());
    CHECK(side_bar_item->geometry().height() == 900);

    placed.object_tree->open();
    CHECK(placed.object_tree->isOpen());
    CHECK(object_tree_item->isVisible());
    const int difference = side_bar_item->geometry().height() - side_bar_height_before;
    CHECK(difference >= -1);
    CHECK(difference <= 1);
}

TEST_CASE("DockCore: dock widgets can tab into the central group")
{
    DockFixture f;
    PlacedDocks placed(f);

    dock::Group* central_group = f.main_window.centralGroup();
    REQUIRE(central_group != nullptr);
    const int tabs_before = central_group->dockWidgets().size();

    dock::DockWidget* extra = f.makeDock(QStringLiteral("extra"), QStringLiteral("附加面板"));
    f.main_window.addDockWidgetAsTab(extra);
    extra->markOpen(true);

    CHECK(central_group->dockWidgets().size() == tabs_before + 1);
    CHECK(central_group->openDockWidgets().contains(extra));

    central_group->setCurrentDockWidget(extra);
    CHECK(central_group->currentDockWidget() == extra);
    CHECK(central_group->currentIndex() == central_group->openDockWidgets().indexOf(extra));
    CHECK(central_group->title() == QStringLiteral("附加面板"));
}

TEST_CASE("DockCore: dock registry tracks dock widgets and main window")
{
    DockFixture f;
    PlacedDocks placed(f);

    CHECK(dock::DockRegistry::self().mainWindow() == &f.main_window);
    CHECK(dock::DockRegistry::self().dockWidgets().contains(placed.object_tree));
    CHECK(dock::DockRegistry::self().dockWidgets().size() == 8); // 7 + 中央持久面板
}
