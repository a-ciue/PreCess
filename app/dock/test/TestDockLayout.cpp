/**
 * @file TestDockLayout.cpp
 * @brief 停靠布局引擎单元测试：占比分配、分隔条拖动、隐显恢复、移除提升、最小尺寸约束
 */

#include "engine/Item.h"
#include "engine/ItemBoxContainer.h"
#include "engine/LayoutingGuest.h"
#include "engine/Separator.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

namespace {

//! @brief 记录几何与可见性的测试 guest
class TestGuest : public dock::LayoutingGuest
{
public:
    explicit TestGuest(QSize min_size = QSize(0, 0),
        QSize max_size_hint = QSize(dock::kMaxSizeLimit, dock::kMaxSizeLimit))
        : min_size_(min_size)
        , max_size_hint_(max_size_hint)
    {
    }

    void setGuestGeometry(const QRect& geometry) override { geometry_ = geometry; }
    QSize minSize() const override { return min_size_; }
    QSize maxSizeHint() const override { return max_size_hint_; }
    void setGuestVisible(bool visible) override { visible_ = visible; }

    const QRect& geometry() const { return geometry_; }
    bool visible() const { return visible_; }

private:
    QSize min_size_;
    QSize max_size_hint_;
    QRect geometry_;
    bool visible_ = true;
};

//! @brief 布局引擎测试夹具：持有 guest 生命周期，节点所有权交给布局树
struct LayoutFixture {
    dock::ItemBoxContainer root { Qt::Horizontal };

    //! @brief 创建带 guest 的叶节点
    dock::Item* makeLeaf(int min_width = 0, int min_height = 0)
    {
        auto guest = std::make_unique<TestGuest>(QSize(min_width, min_height));
        auto* item = new dock::Item(guest.get());
        guests.push_back(std::move(guest));
        return item;
    }

    std::vector<std::unique_ptr<TestGuest>> guests;
};

}

TEST_CASE("DockLayout: preferredSize allocation fills container")
{
    LayoutFixture f;
    f.root.setGeometry(QRect(0, 0, 1000, 600));

    dock::Item* left = f.makeLeaf(100, 100);
    f.root.insertItem(0, left, 250, true);

    // 唯一子节点始终占满容器
    CHECK(left->geometry() == QRect(0, 0, 1000, 600));

    dock::Item* right = f.makeLeaf(100, 100);
    f.root.insertItem(1, right, 400, true);

    CHECK(left->geometry().x() == 0);
    CHECK(right->geometry().x() == 1000 - right->geometry().width());
    CHECK(right->geometry().width() >= 400);
    CHECK(left->geometry().width() + dock::kSeparatorThickness + right->geometry().width() == 1000);
    CHECK(left->geometry().height() == 600);
    CHECK(right->geometry().height() == 600);
    CHECK(f.root.separators().size() == 1);
}

TEST_CASE("DockLayout: separator drag respects minimum sizes")
{
    LayoutFixture f;
    f.root.setGeometry(QRect(0, 0, 1000, 600));

    dock::Item* left = f.makeLeaf(200, 100);
    dock::Item* right = f.makeLeaf(300, 100);
    f.root.insertItem(0, left, 500, true);
    f.root.insertItem(1, right, 500, true);

    REQUIRE(f.root.separators().size() == 1);
    dock::Separator* separator = f.root.separators().first();

    SECTION("drag right")
    {
        REQUIRE(separator->move(600));
        CHECK(left->geometry().width() == 600);
        CHECK(right->geometry().x() == 600 + dock::kSeparatorThickness);
        CHECK(left->geometry().width() + dock::kSeparatorThickness + right->geometry().width() == 1000);
    }

    SECTION("clamped by side 1 minimum")
    {
        REQUIRE(separator->move(-500));
        CHECK(left->geometry().width() == 200);
        CHECK(left->geometry().width() + dock::kSeparatorThickness + right->geometry().width() == 1000);
    }

    SECTION("clamped by side 2 minimum")
    {
        REQUIRE(separator->move(5000));
        CHECK(right->geometry().width() == 300);
        CHECK(left->geometry().width() + dock::kSeparatorThickness + right->geometry().width() == 1000);
    }
}

TEST_CASE("DockLayout: hidden item yields space and restores on show")
{
    LayoutFixture f;
    f.root.setGeometry(QRect(0, 0, 1000, 600));

    dock::Item* left = f.makeLeaf(100, 100);
    dock::Item* right = f.makeLeaf(100, 100);
    f.root.insertItem(0, left, 400, true);
    f.root.insertItem(1, right, 400, true);

    const int right_width_before = right->geometry().width();
    REQUIRE(f.root.separators().size() == 1);

    right->setVisible(false);
    CHECK(f.root.separators().isEmpty());
    CHECK(left->geometry().width() == 1000);
    CHECK_FALSE(right->isVisible());

    right->setVisible(true);
    REQUIRE(f.root.separators().size() == 1);
    CHECK(left->geometry().width() + dock::kSeparatorThickness + right->geometry().width() == 1000);
    const int difference = right->geometry().width() - right_width_before;
    CHECK(difference >= -1);
    CHECK(difference <= 1);
    CHECK(right->geometry().height() == 600);
}

TEST_CASE("DockLayout: single-child container is promoted on removal")
{
    LayoutFixture f;
    f.root.setGeometry(QRect(0, 0, 1000, 600));

    dock::Item* left = f.makeLeaf(100, 100);
    f.root.insertItem(0, left, 300, true);

    auto* column = new dock::ItemBoxContainer(Qt::Vertical);
    dock::Item* top = f.makeLeaf(100, 100);
    dock::Item* bottom = f.makeLeaf(100, 100);
    column->insertItem(0, top, 300, true);
    column->insertItem(1, bottom, 300, true);
    f.root.insertItem(1, column, 700, true);

    REQUIRE(f.root.childCount() == 2);
    REQUIRE(column->childCount() == 2);
    CHECK(top->geometry().width() == column->geometry().width());

    dock::Item* replacement = column->removeItem(bottom, true);
    CHECK(replacement == top);
    CHECK(f.root.childCount() == 2);
    CHECK(f.root.children().at(1) == top);
    delete column;

    CHECK(top->isVisible());
    CHECK(top->geometry().width() > 0);
    CHECK(top->geometry().width() + dock::kSeparatorThickness + left->geometry().width() == 1000);
}

TEST_CASE("DockLayout: minimum sizes overflow when constrained")
{
    LayoutFixture f;
    f.root.setGeometry(QRect(0, 0, 1000, 600));

    dock::Item* left = f.makeLeaf(300, 100);
    dock::Item* right = f.makeLeaf(700, 100);
    f.root.insertItem(0, left, 500, true);
    f.root.insertItem(1, right, 500, true);

    // 最小宽度总和 1005 > 容器宽度 1000：两侧均保持最小宽度
    CHECK(left->geometry().width() == 300);
    CHECK(right->geometry().width() == 700);
    CHECK(f.root.minSize().width() == 1005);
}

TEST_CASE("DockLayout: vertical container uses height as main axis")
{
    LayoutFixture f;
    dock::ItemBoxContainer column { Qt::Vertical };
    column.setGeometry(QRect(10, 20, 400, 800));

    dock::Item* top = f.makeLeaf(100, 100);
    dock::Item* bottom = f.makeLeaf(100, 100);
    column.insertItem(0, top, 300, true);
    column.insertItem(1, bottom, 200, true);

    CHECK(top->geometry().x() == 10);
    CHECK(top->geometry().width() == 400);
    CHECK(top->geometry().y() == 20);
    CHECK(top->geometry().height() + dock::kSeparatorThickness + bottom->geometry().height() == 800);
    CHECK(column.minSize().height() == 205);
    CHECK(column.minSize().width() == 100);
}
