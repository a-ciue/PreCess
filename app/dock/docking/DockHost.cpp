/**
 * @file DockHost.cpp
 * @brief 主停靠窗口的实现
 */

#include "DockHost.h"

#include "DockCatalog.h"
#include "DockPanel.h"
#include "DockRegion.h"
#include "DockWindow.h"
#include "DragSession.h"
#include "PanelGroup.h"
#include "tree/BoxNode.h"
#include "tree/LayoutNode.h"

#include <QDebug>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QList>
#include <QSet>

namespace dock {

namespace {

//! @brief 布局快照版本（结构不兼容变更时递增）
constexpr int kLayoutVersion = 1;

QString orientationToString(Qt::Orientation orientation)
{
    return orientation == Qt::Horizontal ? QStringLiteral("horizontal")
                                         : QStringLiteral("vertical");
}

Qt::Orientation orientationFromString(const QString& value)
{
    return value == QLatin1String("vertical") ? Qt::Vertical : Qt::Horizontal;
}

//! @brief 节点在父容器中的有效占比（隐藏节点取隐藏前保存的占比）
double effectiveShare(const LayoutNode* item)
{
    return item->isVisible() ? item->share() : item->rememberedShare();
}

void collectGroupIds(LayoutNode* item, QHash<PanelGroup*, int>& ids, int& next_id)
{
    if (!item)
        return;

    if (item->isContainer()) {
        for (LayoutNode* child : static_cast<BoxNode*>(item)->children())
            collectGroupIds(child, ids, next_id);
        return;
    }

    if (auto* group = dynamic_cast<PanelGroup*>(item->client())) {
        if (!ids.contains(group))
            ids.insert(group, next_id++);
    }
}

QJsonObject serializeGroup(PanelGroup* group, const QHash<PanelGroup*, int>& ids,
    const PanelGroup* central_group)
{
    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("group"));
    object.insert(QStringLiteral("id"), ids.value(group, -1));
    if (group == central_group)
        object.insert(QStringLiteral("central"), true);

    QJsonArray panels;
    for (DockPanel* panel : group->panels()) {
        QJsonObject entry;
        entry.insert(QStringLiteral("name"), panel->uniqueName());
        entry.insert(QStringLiteral("shown"), panel->isShown());
        panels.append(entry);
    }
    object.insert(QStringLiteral("panels"), panels);

    DockPanel* active = group->activePanel();
    object.insert(QStringLiteral("active"), active ? active->uniqueName() : QString());
    return object;
}

QJsonObject serializeNode(LayoutNode* item, const QHash<PanelGroup*, int>& ids,
    const PanelGroup* central_group, const QHash<LayoutNode*, int>& vacancy_windows)
{
    if (!item)
        return {};

    if (item->isContainer()) {
        auto* container = static_cast<BoxNode*>(item);
        QJsonObject object;
        object.insert(QStringLiteral("type"), QStringLiteral("box"));
        object.insert(QStringLiteral("orientation"), orientationToString(container->orientation()));

        QJsonArray children;
        for (LayoutNode* child : container->children()) {
            QJsonObject entry;
            entry.insert(QStringLiteral("share"), effectiveShare(child));
            entry.insert(QStringLiteral("node"),
                serializeNode(child, ids, central_group, vacancy_windows));
            children.append(entry);
        }
        object.insert(QStringLiteral("children"), children);
        return object;
    }

    if (auto* group = dynamic_cast<PanelGroup*>(item->client()))
        return serializeGroup(group, ids, central_group);

    // 占位（整组浮动留在主树的空节点）：按浮窗下标引用
    const int window_index = vacancy_windows.value(item, -1);
    if (window_index < 0)
        return {};

    QJsonObject object;
    object.insert(QStringLiteral("type"), QStringLiteral("vacancy"));
    object.insert(QStringLiteral("window"), window_index);
    return object;
}

//! @brief 恢复上下文：面板名映射、组 id 映射、占位节点、快照占比
struct RestoreContext {
    DockHost* host = nullptr;
    QHash<QString, DockPanel*> panels;
    QHash<int, PanelGroup*> groups;
    QHash<int, LayoutNode*> vacancies; // 浮窗下标 → 主树占位节点
    QList<QPair<LayoutNode*, double>> saved_shares; // 节点 → 快照占比
    bool central_seen = false;
};

LayoutNode* buildNode(const QJsonObject& object, RestoreContext& ctx, QObject* group_parent,
    bool allow_central);

PanelGroup* buildGroup(const QJsonObject& object, RestoreContext& ctx, QObject* group_parent,
    bool allow_central)
{
    const QJsonArray entries = object.value(QStringLiteral("panels")).toArray();

    // 含中央面板的分组复用现有中央分组与节点（仅主树允许）
    PanelGroup* group = nullptr;
    LayoutNode* node = nullptr;
    if (allow_central) {
        for (const QJsonValue& value : entries) {
            const QString name = value.toObject().value(QStringLiteral("name")).toString();
            if (ctx.panels.value(name, nullptr) == ctx.host->centralPanel()) {
                group = ctx.host->centralGroup();
                node = ctx.host->centralNode();
                ctx.central_seen = true;
                break;
            }
        }
    }

    if (!group) {
        group = new PanelGroup(group_parent);
        node = new LayoutNode(group);
        group->setNode(node);
    }

    for (const QJsonValue& value : entries) {
        const QJsonObject entry = value.toObject();
        DockPanel* panel = ctx.panels.value(entry.value(QStringLiteral("name")).toString(), nullptr);
        if (!panel)
            continue; // 快照中的未知面板：跳过（向前兼容）
        if (panel->group() && panel->group() != group)
            continue; // 快照重复引用：保留首次出现

        group->insertPanel(panel, -1); // 中央面板已在组内时为空操作
        panel->applyDetached(false);
        panel->applyShown(entry.value(QStringLiteral("shown")).toBool(true));
    }

    if (group->panels().isEmpty()) {
        if (group == ctx.host->centralGroup()) {
            ctx.central_seen = false; // 中央分组必须有效
            return nullptr;
        }
        delete node;
        delete group;
        return nullptr;
    }

    // 仅在分组确定有效后登记，避免悬空 id 引用（失败清理与回停来源会按 id 取用）
    const int id = object.value(QStringLiteral("id")).toInt(-1);
    if (id >= 0)
        ctx.groups.insert(id, group);

    const QString active_name = object.value(QStringLiteral("active")).toString();
    if (DockPanel* active = ctx.panels.value(active_name, nullptr))
        group->setActivePanel(active);

    return group;
}

LayoutNode* buildNode(const QJsonObject& object, RestoreContext& ctx, QObject* group_parent,
    bool allow_central)
{
    const QString type = object.value(QStringLiteral("type")).toString();

    if (type == QLatin1String("group")) {
        PanelGroup* group = buildGroup(object, ctx, group_parent, allow_central);
        return group ? group->node() : nullptr;
    }

    if (type == QLatin1String("vacancy")) {
        auto* vacancy = new LayoutNode(nullptr);
        vacancy->setVisibleSilently(false); // 占位不占空间，仅记录回停位置
        const int window_index = object.value(QStringLiteral("window")).toInt(-1);
        if (window_index >= 0)
            ctx.vacancies.insert(window_index, vacancy);
        return vacancy;
    }

    if (type == QLatin1String("box")) {
        auto* box = new BoxNode(
            orientationFromString(object.value(QStringLiteral("orientation")).toString()));
        for (const QJsonValue& value : object.value(QStringLiteral("children")).toArray()) {
            const QJsonObject entry = value.toObject();
            LayoutNode* child = buildNode(entry.value(QStringLiteral("node")).toObject(), ctx,
                group_parent, allow_central);
            if (!child)
                continue;
            box->insertNode(box->childCount(), child, 0, true);
            const double share = entry.value(QStringLiteral("share")).toDouble(0.0);
            // 建树期几何未定，占比可能被最小尺寸钳制：记录后在建树完成时统一回填
            ctx.saved_shares.append({ child, share });
        }
        if (box->childCount() == 0) {
            delete box;
            return nullptr;
        }
        box->rebuildDividers();
        return box;
    }

    return nullptr;
}

//! @brief 快照结构预校验（不改动任何对象；失败时恢复流程直接放弃）
bool validateLayout(const QJsonObject& root, const QString& host_name, const QString& central_name)
{
    if (root.value(QStringLiteral("version")).toInt(-1) != kLayoutVersion)
        return false;
    if (root.value(QStringLiteral("host")).toString() != host_name)
        return false;
    if (root.value(QStringLiteral("central")).toString() != central_name)
        return false;

    const QJsonObject main = root.value(QStringLiteral("main")).toObject();
    if (main.isEmpty())
        return false;

    QHash<int, int> id_uses;
    int central_count = 0;
    int central_outside = 0;
    QSet<int> vacancy_windows;

    const auto walk = [&](auto&& self, const QJsonObject& node, bool in_main) -> void {
        const QString type = node.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("group")) {
            const int id = node.value(QStringLiteral("id")).toInt(-1);
            if (id >= 0)
                ++id_uses[id];
            for (const QJsonValue& value : node.value(QStringLiteral("panels")).toArray()) {
                if (value.toObject().value(QStringLiteral("name")).toString() == central_name) {
                    if (in_main)
                        ++central_count;
                    else
                        ++central_outside;
                }
            }
            return;
        }
        if (type == QLatin1String("vacancy")) {
            if (in_main)
                vacancy_windows.insert(node.value(QStringLiteral("window")).toInt(-1));
            return;
        }
        if (type == QLatin1String("box")) {
            for (const QJsonValue& value : node.value(QStringLiteral("children")).toArray())
                self(self, value.toObject().value(QStringLiteral("node")).toObject(), in_main);
        }
    };
    walk(walk, main, true);

    const QJsonArray windows = root.value(QStringLiteral("windows")).toArray();
    for (int i = 0; i < windows.size(); ++i) {
        const QJsonObject window = windows.at(i).toObject();
        if (window.value(QStringLiteral("geometry")).toArray().size() != 4)
            return false;
        walk(walk, window.value(QStringLiteral("tree")).toObject(), false);

        const bool placeholder = window.value(QStringLiteral("placeholder")).toBool(false);
        if (placeholder != vacancy_windows.contains(i))
            return false;

        if (window.contains(QStringLiteral("origin"))) {
            const int origin_id = window.value(QStringLiteral("origin")).toObject()
                                      .value(QStringLiteral("group"))
                                      .toInt(-1);
            if (origin_id < 0)
                return false;
        }
    }

    // 中央面板必须在主区域出现且仅一次，不得出现在浮窗树
    if (central_count != 1 || central_outside != 0)
        return false;

    // id 必须唯一（占位/回停来源引用依赖）
    for (auto it = id_uses.constBegin(); it != id_uses.constEnd(); ++it) {
        if (it.value() != 1)
            return false;
    }
    return true;
}

} // namespace

DockHost::DockHost(const QString& unique_name, QObject* parent)
    : DockObject(parent)
    , unique_name_(unique_name)
    , region_(new DockRegion(this))
{
    central_panel_ = new DockPanel(unique_name_ + QStringLiteral("-CentralPanel"), this);
    central_panel_->setIsCentral(true);

    central_group_ = new PanelGroup(this);
    central_group_->setIsCentral(true);
    central_group_->addPanel(central_panel_);

    central_node_ = new LayoutNode(central_group_);
    central_node_->setId(central_panel_->uniqueName());
    central_group_->setNode(central_node_);
    region_->setCentralNode(central_node_);
    region_->setRootNode(central_node_);
    central_panel_->applyShown(true);

    DockCatalog::self().setHost(this);
}

DockHost::~DockHost()
{
    // 宿主拆解期静默中止拖拽会话：会话持有的区域/分组/窗口指针随本对象一同失效
    DragSession::self().abort();

    if (DockCatalog::self().host() == this)
        DockCatalog::self().setHost(nullptr);
}

void DockHost::setCentralContentView(DockView* content_view)
{
    central_panel_->setContentView(content_view);
}

void DockHost::placePanel(DockPanel* panel, DockEdge edge, DockPanel* relative_to,
    const QSize& preferred_size, PanelLaunch launch)
{
    region_->placePanel(panel, edge, relative_to, preferred_size, launch);
}

void DockHost::hideOtherGroups(PanelGroup* except)
{
    const QList<DockPanel*> panels = DockCatalog::self().panels();
    for (DockPanel* panel : panels) {
        if (!panel || panel == central_panel_ || !panel->isShown())
            continue;
        // 能力门控：不可关闭的面板不参与“关闭其他组”
        if (!panel->hasFeature(DockPanel::Feature::Closable))
            continue;
        if (except && panel->group() == except)
            continue;
        panel->hidePanel();
    }
}

QByteArray DockHost::saveLayout() const
{
    if (DragSession::self().phase() != DragSession::Phase::Idle)
        DragSession::self().cancel();

    const QList<DockWindow*> windows = DockCatalog::self().windows();

    // 组 id：主区域树与各浮窗树统一编号（供占位/回停来源引用）
    QHash<PanelGroup*, int> ids;
    int next_id = 0;
    collectGroupIds(region_->rootNode(), ids, next_id);
    for (DockWindow* window : windows)
        collectGroupIds(window->region()->rootNode(), ids, next_id);

    // 占位节点 → 浮窗下标
    QHash<LayoutNode*, int> vacancy_windows;
    for (int i = 0; i < windows.size(); ++i) {
        DockWindow* window = windows.at(i);
        if (!window || !window->group())
            continue;
        if (LayoutNode* vacancy = window->group()->vacancy())
            vacancy_windows.insert(vacancy, i);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), kLayoutVersion);
    root.insert(QStringLiteral("host"), unique_name_);
    root.insert(QStringLiteral("central"),
        central_panel_ ? central_panel_->uniqueName() : QString());
    root.insert(QStringLiteral("main"),
        serializeNode(region_->rootNode(), ids, central_group_, vacancy_windows));

    QJsonArray window_entries;
    for (int i = 0; i < windows.size(); ++i) {
        DockWindow* window = windows.at(i);
        if (!window || !window->region())
            continue;

        const QRect geometry = window->geometry();
        QJsonObject entry;
        entry.insert(QStringLiteral("geometry"),
            QJsonArray { geometry.x(), geometry.y(), geometry.width(), geometry.height() });

        PanelGroup* primary = window->group();
        entry.insert(QStringLiteral("primary"), primary ? ids.value(primary, -1) : -1);
        entry.insert(QStringLiteral("placeholder"), primary && primary->vacancy() != nullptr);
        entry.insert(QStringLiteral("tree"),
            serializeNode(window->region()->rootNode(), ids, central_group_, vacancy_windows));

        PanelGroup* origin = nullptr;
        int origin_index = -1;
        if (primary && DragSession::self().parkedOrigin(primary, origin, origin_index)) {
            QJsonObject origin_entry;
            origin_entry.insert(QStringLiteral("group"), ids.value(origin, -1));
            origin_entry.insert(QStringLiteral("index"), origin_index);
            entry.insert(QStringLiteral("origin"), origin_entry);
        }
        window_entries.append(entry);
    }
    root.insert(QStringLiteral("windows"), window_entries);

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

bool DockHost::restoreLayout(const QByteArray& layout)
{
    if (layout.isEmpty())
        return false;

    QJsonParseError error {};
    const QJsonDocument document = QJsonDocument::fromJson(layout, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return false;

    const QJsonObject root = document.object();
    const QString central_name = central_panel_ ? central_panel_->uniqueName() : QString();
    if (!validateLayout(root, unique_name_, central_name))
        return false;

    // 建树前置校验（不改动任何布局状态）：主树必须能按面板身份解析出中央分组，
    // 与 validateLayout 的按名校验互补；不满足时按契约返回 false 且保持当前布局。
    RestoreContext ctx;
    ctx.host = this;
    for (DockPanel* panel : DockCatalog::self().panels()) {
        if (!panel)
            continue;
        ctx.panels.insert(panel->uniqueName(), panel);
    }
    const auto central_resolvable = [&](auto&& self, const QJsonObject& node) -> bool {
        const QString type = node.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("group")) {
            for (const QJsonValue& value : node.value(QStringLiteral("panels")).toArray()) {
                const QString name = value.toObject().value(QStringLiteral("name")).toString();
                if (name == central_name && ctx.panels.value(name, nullptr) == central_panel_)
                    return true;
            }
            return false;
        }
        if (type == QLatin1String("box")) {
            for (const QJsonValue& value : node.value(QStringLiteral("children")).toArray()) {
                if (self(self, value.toObject().value(QStringLiteral("node")).toObject()))
                    return true;
            }
        }
        return false;
    };
    if (!central_resolvable(central_resolvable, root.value(QStringLiteral("main")).toObject()))
        return false;

    if (DragSession::self().phase() != DragSession::Phase::Idle)
        DragSession::self().cancel();

    // 快照未包含的面板：恢复后按原有显隐兜底放置，保证仍可用
    QHash<QString, bool> referenced;
    const auto mark = [&referenced](auto&& self, const QJsonObject& node) -> void {
        const QString type = node.value(QStringLiteral("type")).toString();
        if (type == QLatin1String("group")) {
            for (const QJsonValue& value : node.value(QStringLiteral("panels")).toArray())
                referenced.insert(value.toObject().value(QStringLiteral("name")).toString(), true);
            return;
        }
        if (type == QLatin1String("box")) {
            for (const QJsonValue& value : node.value(QStringLiteral("children")).toArray())
                self(self, value.toObject().value(QStringLiteral("node")).toObject());
        }
    };
    mark(mark, root.value(QStringLiteral("main")).toObject());
    for (const QJsonValue& value : root.value(QStringLiteral("windows")).toArray())
        mark(mark, value.toObject().value(QStringLiteral("tree")).toObject());

    QList<QPair<DockPanel*, bool>> leftovers;
    for (DockPanel* panel : DockCatalog::self().panels()) {
        if (!panel || panel == central_panel_)
            continue;
        if (!referenced.contains(panel->uniqueName()))
            leftovers.append({ panel, panel->isShown() });
    }

    // 清空现有布局（不通知视图：成功恢复由末尾 layoutRestored 统一驱动）
    clearLayoutInternal(false);

    // 建树期间先摘出中央根，避免 setRootNode 误删被复用的中央节点
    region_->takeRootNode();

    LayoutNode* main_root = buildNode(root.value(QStringLiteral("main")).toObject(), ctx, region_,
        true);
    if (!main_root || !ctx.central_seen) {
        for (PanelGroup* group : ctx.groups) {
            if (group == central_group_)
                continue;
            const QList<DockPanel*> panels = group->panels();
            for (DockPanel* panel : panels) {
                panel->applyDetached(false);
                panel->applyShown(false);
                group->removePanel(panel);
            }
            delete group;
        }
        delete main_root;
        // 前置校验后不应到达：防御性重置为默认布局并通知视图
        clearLayoutInternal(true);
        return false;
    }
    region_->setRootNode(main_root);

    const QJsonArray window_entries = root.value(QStringLiteral("windows")).toArray();
    for (int i = 0; i < window_entries.size(); ++i) {
        const QJsonObject entry = window_entries.at(i).toObject();
        DockWindow* window = DragSession::self().createFloatingWindow();
        if (!window)
            continue;

        LayoutNode* tree = buildNode(entry.value(QStringLiteral("tree")).toObject(), ctx,
            window->region(), false);
        PanelGroup* primary
            = ctx.groups.value(entry.value(QStringLiteral("primary")).toInt(-1), nullptr);

        // primary 必须是本窗口树的节点：否则窗口主分组指向别处节点，
        // 后续 releaseGroup/takeGroupNode 会从其他区域摘走节点破坏布局
        if (primary) {
            bool in_tree = false;
            for (LayoutNode* item = primary->node(); item; item = item->parent()) {
                if (item == tree) {
                    in_tree = true;
                    break;
                }
            }
            if (!in_tree)
                primary = nullptr;
        }
        window->adoptTree(primary, tree);

        const QJsonArray geometry = entry.value(QStringLiteral("geometry")).toArray();
        window->setGeometry(QRect(geometry.at(0).toInt(), geometry.at(1).toInt(),
            geometry.at(2).toInt(), geometry.at(3).toInt()));

        if (primary && entry.value(QStringLiteral("placeholder")).toBool(false)) {
            if (LayoutNode* vacancy = ctx.vacancies.value(i, nullptr))
                primary->setVacancy(vacancy);
        }

        if (primary && entry.contains(QStringLiteral("origin"))) {
            const QJsonObject origin = entry.value(QStringLiteral("origin")).toObject();
            PanelGroup* origin_group
                = ctx.groups.value(origin.value(QStringLiteral("group")).toInt(-1), nullptr);
            if (origin_group)
                DragSession::self().restoreParkedGroup(primary, origin_group,
                    origin.value(QStringLiteral("index")).toInt(-1));
        }
    }

    // 回填快照占比：建树期的临时几何可能已按最小尺寸钳制占比
    for (const auto& pair : ctx.saved_shares) {
        if (pair.second > 0.0) {
            pair.first->setShare(pair.second);
            pair.first->setRememberedShare(pair.second);
        }
    }
    // 强制按最终占比重排（区域几何未变时 setGeometry 仍会触发布局）
    region_->setGeometry(region_->geometry());
    for (DockWindow* window : DockCatalog::self().windows())
        window->region()->setGeometry(window->region()->geometry());

    // 快照未包含的面板：按原有显隐兜底停靠到主区域底部
    for (const auto& leftover : leftovers) {
        region_->placePanel(leftover.first, DockEdge::Bottom, nullptr, QSize(0, 300),
            leftover.second ? PanelLaunch::Visible : PanelLaunch::Hidden);
    }

    Q_EMIT layoutRestored();
    return true;
}

void DockHost::clearLayout()
{
    clearLayoutInternal(true);
}

void DockHost::clearLayoutInternal(bool notify)
{
    if (DragSession::self().phase() != DragSession::Phase::Idle)
        DragSession::self().cancel();

    // 销毁全部浮动窗口（浮窗内的分组与节点随其区域析构）
    const QList<DockWindow*> windows = DockCatalog::self().windows();
    for (DockWindow* window : windows)
        DragSession::self().destroyFloatingWindow(window);

    // 主区域：收集分组
    const QList<PanelGroup*> groups = region_->groups();

    // 先摘出中央节点并删除其余树（此时所有节点 client 仍有效，避免布局期悬空）
    if (central_node_) {
        if (BoxNode* parent = central_node_->parent()) {
            LayoutNode* result = parent->detachNode(central_node_, false);
            if (result != parent)
                delete parent; // 容器被替代或摘空：原容器已脱离树，需回收
        }
    }
    LayoutNode* root = region_->takeRootNode();
    if (root && root != central_node_)
        delete root;

    // 非中央分组：节点已随树删除，解除引用后归一化面板并回收分组
    for (PanelGroup* group : groups) {
        if (group == central_group_)
            continue;

        group->setNode(nullptr);
        const QList<DockPanel*> panels = group->panels();
        for (DockPanel* panel : panels) {
            panel->applyDetached(false);
            panel->applyShown(false);
            group->removePanel(panel);
        }
        delete group;
    }

    // 中央分组：仅保留中央面板
    if (central_group_) {
        const QList<DockPanel*> panels = central_group_->panels();
        for (DockPanel* panel : panels) {
            if (panel == central_panel_)
                continue;
            panel->applyDetached(false);
            panel->applyShown(false);
            central_group_->removePanel(panel);
        }
    }

    // 重建仅含中央面板的布局
    if (central_node_ && central_group_) {
        central_group_->setNode(central_node_);
        central_node_->setParent(nullptr);
        central_panel_->applyDetached(false);
        central_panel_->applyShown(true);
        region_->setCentralNode(central_node_);
        region_->setRootNode(central_node_);
    }

    if (notify)
        Q_EMIT layoutRestored();
}

void DockHost::setFrame(const QRect& frame)
{
    region_->setGeometry(frame);
}

}
