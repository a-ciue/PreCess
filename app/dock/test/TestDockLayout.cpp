/**
 * @file TestDockLayout.cpp
 * @brief 停靠布局引擎单元测试：占比分配、分隔条拖动、隐显恢复、移除提升、最小尺寸约束
 */

#include "tree/LayoutNode.h"
#include "tree/BoxNode.h"
#include "tree/LayoutClient.h"
#include "tree/Divider.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <vector>

namespace {

//! @brief 记录几何与可见性的测试 client
class TestGuest : public dock::LayoutClient
{
public:
    explicit TestGuest(QSize min_size = QSize(0, 0),
        QSize max_size_hint = QSize(dock::kMaxSizeLimit, dock::kMaxSizeLimit))
        : min_size_(min_size)
        , max_size_hint_(max_size_hint)
    {
    }

    void applyGeometry(const QRect& geometry) override { geometry_ = geometry; }
    QSize minExtent() const override { return min_size_; }
    QSize maxExtent() const override { return max_size_hint_; }
    void applyVisibility(bool visible) override { visible_ = visible; }

    const QRect& geometry() const { return geometry_; }
    bool visible() const { return visible_; }

private:
    QSize min_size_;
    QSize max_size_hint_;
    QRect geometry_;
    bool visible_ = true;
};

//! @brief 布局引擎测试夹具：持有 client 生命周期，节点所有权交给布局树
struct LayoutFixture {
    dock::BoxNode root { Qt::Horizontal };

    //! @brief 创建带 client 的叶节点
    dock::LayoutNode* makeLeaf(int min_width = 0, int min_height = 0)
    {
        auto client = std::make_unique<TestGuest>(QSize(min_width, min_height));
        auto* item = new dock::LayoutNode(client.get());
        guests.push_back(std::move(client));
        return item;
    }

    std::vector<std::unique_ptr<TestGuest>> guests;
};

}

TEST_CASE("DockLayout: preferredSize allocation fills container")
{
    LayoutFixture f;
    f.root.setGeometry(QRect(0, 0, 1000, 600));

    dock::LayoutNode* left = f.makeLeaf(100, 100);
    f.root.insertNode(0, left, 250, true);

    // 唯一子节点始终占满容器
    CHECK(left->geometry() == QRect(0, 0, 1000, 600));

    dock::LayoutNode* right = f.makeLeaf(100, 100);
    f.root.insertNode(1, right, 400, true);

    CHECK(left->geometry().x() == 0);
    CHECK(right->geometry().x() == 1000 - right->geometry().width());
    CHECK(right->geometry().width() >= 400);
    CHECK(left->geometry().width() + dock::kDividerThickness + right->geometry().width() == 1000);
    CHECK(left->geometry().height() == 600);
    CHECK(right->geometry().height() == 600);
    CHECK(f.root.dividers().size() == 1);
}

TEST_CASE("DockLayout: separator drag respects minimum sizes")
{
    LayoutFixture f;
    f.root.setGeometry(QRect(0, 0, 1000, 600));

    dock::LayoutNode* left = f.makeLeaf(200, 100);
    dock::LayoutNode* right = f.makeLeaf(300, 100);
    f.root.insertNode(0, left, 500, true);
    f.root.insertNode(1, right, 500, true);

    REQUIRE(f.root.dividers().size() == 1);
    dock::Divider* separator = f.root.dividers().first();

    SECTION("drag right")
    {
        REQUIRE(separator->move(600));
        CHECK(left->geometry().width() == 600);
        CHECK(right->geometry().x() == 600 + dock::kDividerThickness);
        CHECK(left->geometry().width() + dock::kDividerThickness + right->geometry().width() == 1000);
    }

    SECTION("clamped by side 1 minimum")
    {
        REQUIRE(separator->move(-500));
        CHECK(left->geometry().width() == 200);
        CHECK(left->geometry().width() + dock::kDividerThickness + right->geometry().width() == 1000);
    }

    SECTION("clamped by side 2 minimum")
    {
        REQUIRE(separator->move(5000));
        CHECK(right->geometry().width() == 300);
        CHECK(left->geometry().width() + dock::kDividerThickness + right->geometry().width() == 1000);
    }
}

TEST_CASE("DockLayout: hidden item yields space and restores on show")
{
    LayoutFixture f;
    f.root.setGeometry(QRect(0, 0, 1000, 600));

    dock::LayoutNode* left = f.makeLeaf(100, 100);
    dock::LayoutNode* right = f.makeLeaf(100, 100);
    f.root.insertNode(0, left, 400, true);
    f.root.insertNode(1, right, 400, true);

    const int right_width_before = right->geometry().width();
    REQUIRE(f.root.dividers().size() == 1);

    right->setVisible(false);
    CHECK(f.root.dividers().isEmpty());
    CHECK(left->geometry().width() == 1000);
    CHECK_FALSE(right->isVisible());

    right->setVisible(true);
    REQUIRE(f.root.dividers().size() == 1);
    CHECK(left->geometry().width() + dock::kDividerThickness + right->geometry().width() == 1000);
    const int difference = right->geometry().width() - right_width_before;
    CHECK(difference >= -1);
    CHECK(difference <= 1);
    CHECK(right->geometry().height() == 600);
}

TEST_CASE("DockLayout: single-child container is promoted on removal")
{
    LayoutFixture f;
    f.root.setGeometry(QRect(0, 0, 1000, 600));

    dock::LayoutNode* left = f.makeLeaf(100, 100);
    f.root.insertNode(0, left, 300, true);

    auto* column = new dock::BoxNode(Qt::Vertical);
    dock::LayoutNode* top = f.makeLeaf(100, 100);
    dock::LayoutNode* bottom = f.makeLeaf(100, 100);
    column->insertNode(0, top, 300, true);
    column->insertNode(1, bottom, 300, true);
    f.root.insertNode(1, column, 700, true);

    REQUIRE(f.root.childCount() == 2);
    REQUIRE(column->childCount() == 2);
    CHECK(top->geometry().width() == column->geometry().width());

    dock::LayoutNode* replacement = column->detachNode(bottom, true);
    CHECK(replacement == top);
    CHECK(f.root.childCount() == 2);
    CHECK(f.root.children().at(1) == top);
    delete column;

    CHECK(top->isVisible());
    CHECK(top->geometry().width() > 0);
    CHECK(top->geometry().width() + dock::kDividerThickness + left->geometry().width() == 1000);
}

TEST_CASE("DockLayout: minimum sizes overflow when constrained")
{
    LayoutFixture f;
    f.root.setGeometry(QRect(0, 0, 1000, 600));

    dock::LayoutNode* left = f.makeLeaf(300, 100);
    dock::LayoutNode* right = f.makeLeaf(700, 100);
    f.root.insertNode(0, left, 500, true);
    f.root.insertNode(1, right, 500, true);

    // 最小宽度总和 1005 > 容器宽度 1000：两侧均保持最小宽度
    CHECK(left->geometry().width() == 300);
    CHECK(right->geometry().width() == 700);
    CHECK(f.root.minExtent().width() == 1005);
}

TEST_CASE("DockLayout: vertical container uses height as main axis")
{
    LayoutFixture f;
    dock::BoxNode column { Qt::Vertical };
    column.setGeometry(QRect(10, 20, 400, 800));

    dock::LayoutNode* top = f.makeLeaf(100, 100);
    dock::LayoutNode* bottom = f.makeLeaf(100, 100);
    column.insertNode(0, top, 300, true);
    column.insertNode(1, bottom, 200, true);

    CHECK(top->geometry().x() == 10);
    CHECK(top->geometry().width() == 400);
    CHECK(top->geometry().y() == 20);
    CHECK(top->geometry().height() + dock::kDividerThickness + bottom->geometry().height() == 800);
    CHECK(column.minExtent().height() == 205);
    CHECK(column.minExtent().width() == 100);
}
