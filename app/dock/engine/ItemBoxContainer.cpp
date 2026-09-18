/**
 * @file ItemBoxContainer.cpp
 * @brief 盒式布局容器的实现
 */

#include "ItemBoxContainer.h"

#include "LayoutingGuest.h"
#include "Separator.h"

#include <algorithm>
#include <utility>

namespace dock {

namespace {

/**
 * @brief 把 amount 像素按容量补足到 values
 *
 * 取整余数优先由尺寸最大的节点吸收（weights 降序），保证期望尺寸较小
 * 的节点（如 preferredSize 指定的面板）不被逐像素的余数放大。
 */
void distributeRemainder(QList<int>& values, const QList<int>& capacities,
    const QList<int>& weights, int amount)
{
    if (amount <= 0)
        return;

    long long total_capacity = 0;
    for (int capacity : capacities)
        total_capacity += capacity;
    if (total_capacity <= 0)
        return;

    int remaining = static_cast<int>(std::min<long long>(amount, total_capacity));

    QList<int> order;
    order.reserve(values.size());
    for (int i = 0; i < values.size(); ++i)
        order.append(i);
    std::stable_sort(order.begin(), order.end(), [&weights](int lhs, int rhs) {
        return weights[lhs] > weights[rhs];
    });

    for (int index : std::as_const(order)) {
        if (remaining <= 0)
            break;
        const int take = std::min(remaining, capacities[index]);
        values[index] += take;
        remaining -= take;
    }
}

/**
 * @brief 将 lengths 收敛到恰好等于 avail（受 min/max 约束）
 *
 * 先按占比取整并 clamp 到约束，再按容量分摊多余/不足的长度；
 * 约束总和与 avail 冲突时允许溢出（由上层窗口最小尺寸兜底）。
 */
void resolveLengths(QList<int>& lengths, const QList<QSize>& mins, const QList<QSize>& maxes,
    int avail, bool horizontal)
{
    const int count = lengths.size();
    QList<int> min_lengths;
    QList<int> max_lengths;
    min_lengths.reserve(count);
    max_lengths.reserve(count);

    int sum = 0;
    for (int i = 0; i < count; ++i) {
        const int min_length = horizontal ? mins[i].width() : mins[i].height();
        const int max_length = std::max(min_length, horizontal ? maxes[i].width() : maxes[i].height());
        min_lengths.append(min_length);
        max_lengths.append(max_length);
        lengths[i] = std::clamp(lengths[i], min_length, max_length);
        sum += lengths[i];
    }

    if (sum < avail) {
        QList<int> capacities;
        capacities.reserve(count);
        for (int i = 0; i < count; ++i)
            capacities.append(std::max(0, max_lengths[i] - lengths[i]));
        distributeRemainder(lengths, capacities, lengths, avail - sum);
    } else if (sum > avail) {
        QList<int> capacities;
        QList<int> shrink(count, 0);
        capacities.reserve(count);
        for (int i = 0; i < count; ++i)
            capacities.append(std::max(0, lengths[i] - min_lengths[i]));
        distributeRemainder(shrink, capacities, lengths, sum - avail);
        for (int i = 0; i < count; ++i)
            lengths[i] -= shrink[i];
    }
}

}

ItemBoxContainer::ItemBoxContainer(Qt::Orientation orientation)
    : Item(nullptr)
    , orientation_(orientation)
{
}

ItemBoxContainer::~ItemBoxContainer()
{
    qDeleteAll(children_);
    qDeleteAll(separators_);
}

int ItemBoxContainer::visibleChildCount() const
{
    int count = 0;
    for (Item* child : children_) {
        if (child->isVisible())
            ++count;
    }
    return count;
}

QList<Item*> ItemBoxContainer::visibleChildren() const
{
    QList<Item*> result;
    for (Item* child : children_) {
        if (child->isVisible())
            result.append(child);
    }
    return result;
}

void ItemBoxContainer::insertItem(int index, Item* item, int preferredLength, bool startsVisible)
{
    if (!item)
        return;

    if (!startsVisible) {
        // 先摘除可见性再挂树，避免触发对既有可见子节点的占比缩放
        item->setVisible(false);
        item->setPercentage(0.0);
    }

    index = std::clamp(index, 0, static_cast<int>(children_.size()));
    children_.insert(index, item);
    item->setParent(this);

    if (startsVisible) {
        const int count = visibleChildCount();
        const int avail = availableLength();
        const int desired = preferredLength > 0 ? preferredLength
                                                : (count > 0 ? avail / count : avail);
        allocatePreferredLength(item, desired);
    } else {
        const int avail = availableLength();
        const double percentage = (preferredLength > 0 && avail > 0)
            ? std::clamp(static_cast<double>(preferredLength) / avail, 0.0, 1.0)
            : 1.0 / std::max(1, visibleChildCount());
        item->setStoredPercentage(percentage);
    }

    refreshVisibility();
    updateSeparators();
    layout();
}

Item* ItemBoxContainer::removeItem(Item* item, bool hardDelete)
{
    const int index = children_.indexOf(item);
    if (index < 0)
        return this;

    const double removed_percentage = item->percentage();
    children_.removeAt(index);
    item->setParent(nullptr);
    if (hardDelete)
        delete item;

    if (children_.isEmpty()) {
        if (parent_) {
            // 非根容器清空：从父容器摘除，由调用方删除本对象
            parent_->children_.removeOne(this);
            setParent(nullptr);
            return nullptr;
        }
        refreshVisibility();
        updateSeparators();
        layout();
        return this;
    }

    if (children_.size() == 1 && parent_) {
        // 只剩一个子节点：用该子节点替换本容器，避免无意义的单子容器层级
        Item* only = children_.first();
        children_.removeAt(0);
        const double child_percentage = only->isVisible() ? only->percentage() : only->storedPercentage();
        const double combined = std::clamp(child_percentage * percentage(), 0.0, 1.0);
        only->setStoredPercentage(combined);
        only->setPercentage(only->isVisible() ? combined : 0.0);
        only->setParent(parent_);

        ItemBoxContainer* host = parent_;
        const int my_index = host->children_.indexOf(this);
        if (my_index >= 0)
            host->children_[my_index] = only;
        setParent(nullptr);

        host->updateSeparators();
        host->layout();
        return only;
    }

    // 其余可见子节点按原占比放大补位
    if (removed_percentage > 0.0 && removed_percentage < 1.0) {
        for (Item* child : children_) {
            if (child->isVisible())
                child->setPercentage(child->percentage() / (1.0 - removed_percentage));
        }
    }

    refreshVisibility();
    updateSeparators();
    layout();
    return this;
}

void ItemBoxContainer::replaceChild(Item* old_child, Item* new_child)
{
    if (!old_child || !new_child || old_child == new_child)
        return;

    const int index = children_.indexOf(old_child);
    if (index < 0)
        return;

    new_child->setParent(this);
    new_child->setVisibleSilently(old_child->isVisible());
    // old_child 通常已被调用方移入 new_child；仅当仍指向本容器时才摘除
    if (old_child->parent() == this)
        old_child->setParent(nullptr);
    children_[index] = new_child;

    updateSeparators();
    layout();
}

void ItemBoxContainer::setGeometry(const QRect& geometry)
{
    sizing_.geometry = geometry;
    if (guest_)
        guest_->setGuestGeometry(geometry);
    if (visible_)
        layout();
}

QSize ItemBoxContainer::minSize() const
{
    const QList<Item*> visible = visibleChildren();
    if (visible.isEmpty())
        return QSize(0, 0);

    const bool horizontal = orientation_ == Qt::Horizontal;
    int main_length = 0;
    int cross_length = 0;
    for (Item* child : visible) {
        const QSize child_min = child->minSize();
        main_length += horizontal ? child_min.width() : child_min.height();
        cross_length = std::max(cross_length, horizontal ? child_min.height() : child_min.width());
    }
    if (visible.size() > 1)
        main_length += (visible.size() - 1) * kSeparatorThickness;

    return horizontal ? QSize(main_length, cross_length) : QSize(cross_length, main_length);
}

QSize ItemBoxContainer::maxSizeHint() const
{
    const QList<Item*> visible = visibleChildren();
    if (visible.isEmpty())
        return QSize(kMaxSizeLimit, kMaxSizeLimit);

    const bool horizontal = orientation_ == Qt::Horizontal;
    int main_length = 0;
    int cross_length = kMaxSizeLimit;
    for (Item* child : visible) {
        const QSize child_max = child->maxSizeHint();
        main_length = static_cast<int>(std::min<long long>(
            static_cast<long long>(main_length) + (horizontal ? child_max.width() : child_max.height()),
            kMaxSizeLimit));
        cross_length = std::min(cross_length, horizontal ? child_max.height() : child_max.width());
    }
    if (visible.size() > 1)
        main_length = static_cast<int>(std::min<long long>(
            static_cast<long long>(main_length) + (visible.size() - 1) * kSeparatorThickness, kMaxSizeLimit));

    return horizontal ? QSize(main_length, cross_length) : QSize(cross_length, main_length);
}

void ItemBoxContainer::layout()
{
    if (!visible_)
        return;

    const QList<Item*> visible = visibleChildren();
    const bool horizontal = orientation_ == Qt::Horizontal;
    const int total_length = sizing_.length(orientation_);
    const int separator_count = visible.size() > 1 ? visible.size() - 1 : 0;
    const int avail = std::max(0, total_length - separator_count * kSeparatorThickness);

    QList<int> lengths;
    QList<QSize> mins;
    QList<QSize> maxes;
    lengths.reserve(visible.size());
    mins.reserve(visible.size());
    maxes.reserve(visible.size());
    for (Item* child : visible) {
        lengths.append(avail > 0 ? qRound(child->percentage() * avail) : 0);
        mins.append(child->minSize());
        maxes.append(child->maxSizeHint());
    }
    resolveLengths(lengths, mins, maxes, avail, horizontal);

    int position = horizontal ? sizing_.geometry.x() : sizing_.geometry.y();
    for (int i = 0; i < visible.size(); ++i) {
        const QRect child_geometry = horizontal
            ? QRect(position, sizing_.geometry.y(), lengths[i], std::max(0, sizing_.geometry.height()))
            : QRect(sizing_.geometry.x(), position, std::max(0, sizing_.geometry.width()), lengths[i]);
        visible[i]->setGeometry(child_geometry);
        position += lengths[i] + kSeparatorThickness;
    }

    // 占比回归实际长度，避免取整误差累积
    if (avail > 0) {
        for (int i = 0; i < visible.size(); ++i)
            visible[i]->setPercentage(static_cast<double>(lengths[i]) / avail);
    }

    for (int i = 0; i < separators_.size() && i + 1 < visible.size(); ++i) {
        const QRect& first = visible[i]->geometry();
        const QRect separator_geometry = horizontal
            ? QRect(first.x() + first.width(), sizing_.geometry.y(), kSeparatorThickness,
                  std::max(0, sizing_.geometry.height()))
            : QRect(sizing_.geometry.x(), first.y() + first.height(), std::max(0, sizing_.geometry.width()),
                  kSeparatorThickness);
        separators_[i]->setGeometry(separator_geometry);
    }
}

void ItemBoxContainer::updateSeparators()
{
    qDeleteAll(separators_);
    separators_.clear();

    const QList<Item*> visible = visibleChildren();
    for (int i = 0; i + 1 < visible.size(); ++i)
        separators_.append(new Separator(this, visible[i], visible[i + 1]));
}

void ItemBoxContainer::applySeparatorMove(Separator* separator, int side1Length, int side2Length)
{
    if (!separator)
        return;

    const int avail = availableLength();
    if (avail > 0) {
        separator->side1()->setPercentage(
            std::clamp(static_cast<double>(side1Length) / avail, 0.0, 1.0));
        separator->side2()->setPercentage(
            std::clamp(static_cast<double>(side2Length) / avail, 0.0, 1.0));
    }
    layout();
}

void ItemBoxContainer::onChildVisibilityChanged(Item* child, bool visible)
{
    if (visible) {
        double percentage = child->storedPercentage();
        if (percentage <= 0.0)
            percentage = 1.0 / std::max(1, visibleChildCount());
        const double others = std::max(0.0, 1.0 - percentage);
        for (Item* other : children_) {
            if (other != child && other->isVisible())
                other->setPercentage(other->percentage() * others);
        }
        child->setPercentage(percentage);
    } else {
        const double hidden_percentage = child->percentage();
        child->setStoredPercentage(hidden_percentage);
        const double others = 1.0 - hidden_percentage;
        if (others > 0.0 && others < 1.0) {
            for (Item* other : children_) {
                if (other != child && other->isVisible())
                    other->setPercentage(other->percentage() / others);
            }
        }
        child->setPercentage(0.0);
    }

    refreshVisibility();
    updateSeparators();
    layout();
}

int ItemBoxContainer::availableLength() const
{
    const int total_length = sizing_.length(orientation_);
    const int count = visibleChildCount();
    const int separator_length = count > 1 ? (count - 1) * kSeparatorThickness : 0;
    return std::max(0, total_length - separator_length);
}

void ItemBoxContainer::allocatePreferredLength(Item* item, int preferredLength)
{
    const int avail = availableLength();
    if (avail <= 0)
        return;

    const bool horizontal = orientation_ == Qt::Horizontal;
    const QSize item_min = item->minSize();
    const QSize item_max = item->maxSizeHint();
    const int min_length = horizontal ? item_min.width() : item_min.height();
    const int max_length = std::max(min_length, horizontal ? item_max.width() : item_max.height());

    int others_min = 0;
    int others_length = 0;
    for (Item* other : children_) {
        if (other == item || !other->isVisible())
            continue;
        const QSize other_min = other->minSize();
        others_min += horizontal ? other_min.width() : other_min.height();
        others_length += other->length(orientation_);
    }

    const int upper_bound = std::max(min_length, avail - others_min);
    const int desired = std::clamp(preferredLength, min_length, std::min(max_length, upper_bound));

    // 其余可见子节点按比例让出 desired 所占份额
    const int others_avail = avail - desired;
    if (others_length > 0) {
        const double scale = static_cast<double>(others_avail) / others_length;
        for (Item* other : children_) {
            if (other != item && other->isVisible())
                other->setPercentage(other->percentage() * scale);
        }
    }
    item->setPercentage(static_cast<double>(desired) / avail);
}

void ItemBoxContainer::refreshVisibility()
{
    if (!parent_)
        return; // 根容器恒可见

    const bool any_visible = !visibleChildren().isEmpty();
    if (any_visible == visible_)
        return;

    visible_ = any_visible;
    parent_->onChildVisibilityChanged(this, any_visible);
}

}
