#include "TreeModel.h"
#include "QModelQuery.h"

#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <algorithm>
#include <array>
#include <functional>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
TreeNode* findChildOfType(TreeNode* parent, TreeNode::NodeType type)
{
    if (!parent)
        return nullptr;
    for (TreeNode* child : parent->children) {
        if (child->nodeType == type)
            return child;
    }
    return nullptr;
}

struct ChildTypeName {
    TreeNode::NodeType type;
    const char* name;
};

constexpr ChildTypeName kMeshChildren[] = {
    { TreeNode::NodeType::D2, "2D" },
    { TreeNode::NodeType::D3, "3D" },
};

constexpr ChildTypeName kGeometryChildren[] = {
    { TreeNode::NodeType::Vertex, "Vertex" },
    { TreeNode::NodeType::Edge, "Edge" },
    { TreeNode::NodeType::Face, "Face" },
    { TreeNode::NodeType::Solid, "Solid" },
};
}

TreeModel::TreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{
    rootNode = new TreeNode("root");
    rootNode->nodeId = -1;

    refresh_timer_ = new QTimer(this);
    refresh_timer_->setSingleShot(true);
    refresh_timer_->setInterval(50);
    connect(refresh_timer_, &QTimer::timeout, this, &TreeModel::flushPendingRefresh);
}

TreeModel::~TreeModel()
{
    delete rootNode;
}

QModelIndex TreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    TreeNode* parentNode = getNode(parent);

    if (row < 0 || row >= parentNode->children.size())
        return QModelIndex();

    return createIndex(row, column, parentNode->children[row]);
}

QModelIndex TreeModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        return QModelIndex();

    TreeNode* childNode = static_cast<TreeNode*>(child.internalPointer());
    TreeNode* parentNode = childNode->parent;

    if (!parentNode || parentNode == rootNode)
        return QModelIndex();

    TreeNode* grandParent = parentNode->parent;
    int row = grandParent->children.indexOf(parentNode);
    return createIndex(row, 0, parentNode);
}

int TreeModel::rowCount(const QModelIndex& parent) const
{
    TreeNode* parentNode = getNode(parent);
    return parentNode->children.size();
}

int TreeModel::columnCount(const QModelIndex&) const
{
    return 1;
}

QVariant TreeModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    TreeNode* node = static_cast<TreeNode*>(index.internalPointer());

    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return node->name;

    case NumberRole:
        return node->number;

    case NodeIdRole:
        return node->nodeId;

    case IsVisibleRole:
        return visibilityState(node) != Qt::Unchecked;
    case VisibilityStateRole:
        return visibilityState(node);
    case CanToggleVisibilityRole:
        return node->nodeType == TreeNode::NodeType::Model
            || node->nodeType == TreeNode::NodeType::Component
            || node->nodeType == TreeNode::NodeType::Mesh
            || node->nodeType == TreeNode::NodeType::Geometry;

    case ComponentIdRole: {
        TreeNode* cur = node;
        while (cur && cur->parent && cur->parent != rootNode) {
            if (cur->parent->parent == rootNode)
                return cur->nodeId;
            cur = cur->parent;
        }
        return -1;
    }

    case NodeTypeRole:
        return static_cast<int>(node->nodeType);

    default:
        return QVariant();
    }
}

QHash<int, QByteArray> TreeModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole]     = "name";
    roles[NumberRole]   = "number";
    roles[NodeIdRole]   = "nodeId";
    roles[IsVisibleRole] = "isVisible";
    roles[ComponentIdRole] = "componentId";
    roles[NodeTypeRole] = "nodeType";
    roles[VisibilityStateRole] = "visibilityState";
    roles[CanToggleVisibilityRole] = "canToggleVisibility";
    return roles;
}

TreeNode* TreeModel::getNode(const QModelIndex& index) const
{
    if (index.isValid())
        return static_cast<TreeNode*>(index.internalPointer());
    return rootNode;
}

bool TreeModel::refresh()
{
    if (!modelQuery_)
        return false;

    if (refresh_timer_ && refresh_timer_->isActive())
        refresh_timer_->stop();
    refresh_pending_ = false;

    refreshing_ = true;
    reconcileAllFromQuery();
    refreshing_ = false;

    // 对账期间收到的新通知留到下一轮定时器消费。
    if (refresh_pending_ && refresh_timer_ && !refresh_timer_->isActive())
        refresh_timer_->start();
    return true;
}

void TreeModel::requestRefresh()
{
    if (!modelQuery_)
        return;

    refresh_pending_ = true;
    if (!refreshing_ && refresh_timer_ && !refresh_timer_->isActive())
        refresh_timer_->start();
}

int TreeModel::meshChildCount(const MeshNodeInfo& info, TreeNode::NodeType type)
{
    switch (type) {
    case TreeNode::NodeType::D2:
        return info.face_count;
    case TreeNode::NodeType::D3:
        return info.solid_count;
    default:
        Q_ASSERT(false);
        return 0;
    }
}

int TreeModel::geometryChildCount(const GeometryNodeInfo& info, TreeNode::NodeType type)
{
    switch (type) {
    case TreeNode::NodeType::Vertex:
        return info.vertex_count;
    case TreeNode::NodeType::Edge:
        return info.edge_count;
    case TreeNode::NodeType::Face:
        return info.face_count;
    case TreeNode::NodeType::Solid:
        return info.solid_count;
    default:
        Q_ASSERT(false);
        return 0;
    }
}

void TreeModel::fillComponentData(ComponentNodeInfo& info) const
{
    if (info.has_mesh) {
        const QVariantMap mesh = modelQuery_->getMeshSummary(info.id);
        info.mesh.face_count = mesh["face_count"].toInt();
        info.mesh.solid_count = mesh["solid_count"].toInt();
    }
    if (info.has_geometry) {
        const QVariantMap geometry = modelQuery_->getGeometrySummary(info.id);
        info.geometry.vertex_count = geometry["vertex_count"].toInt();
        info.geometry.edge_count = geometry["edge_count"].toInt();
        info.geometry.face_count = geometry["face_count"].toInt();
        info.geometry.solid_count = geometry["solid_count"].toInt();
    }
}

TreeModel::ModelNodeInfo TreeModel::queryModel(Index model_id) const
{
    ModelNodeInfo info;
    info.id = model_id;
    if (!modelQuery_)
        return info;

    info.name = modelQuery_->getModelName(model_id);
    const QVariantList components = modelQuery_->getComponentsSummary(model_id);
    info.component_count = components.size();
    info.components.reserve(components.size());
    for (const QVariant& value : components) {
        const QVariantMap component = value.toMap();
        ComponentNodeInfo component_info;
        component_info.id = component["component_id"].toInt();
        component_info.name = component["name"].toString();
        component_info.has_mesh = component["has_mesh"].toBool();
        component_info.has_geometry = component["has_geometry"].toBool();
        fillComponentData(component_info);
        info.components.push_back(std::move(component_info));
    }
    return info;
}

TreeNode* TreeModel::buildMeshNode(const MeshNodeInfo& info)
{
    auto* node = new TreeNode("Mesh", QString::number(info.face_count + info.solid_count));
    node->nodeId = next_fake_id_--;
    node->nodeType = TreeNode::NodeType::Mesh;

    for (const ChildTypeName& spec : kMeshChildren) {
        const int count = meshChildCount(info, spec.type);
        if (count <= 0)
            continue;
        auto* child = new TreeNode(spec.name, QString::number(count), node);
        child->nodeId = next_fake_id_--;
        child->nodeType = spec.type;
    }
    return node;
}

TreeNode* TreeModel::buildGeometryNode(const GeometryNodeInfo& info)
{
    auto* node = new TreeNode("Geometry",
        QString::number(info.vertex_count + info.edge_count + info.face_count + info.solid_count));
    node->nodeId = next_fake_id_--;
    node->nodeType = TreeNode::NodeType::Geometry;

    for (const ChildTypeName& spec : kGeometryChildren) {
        const int count = geometryChildCount(info, spec.type);
        if (count <= 0)
            continue;
        auto* child = new TreeNode(spec.name, QString::number(count), node);
        child->nodeId = next_fake_id_--;
        child->nodeType = spec.type;
    }
    return node;
}

TreeNode* TreeModel::buildComponentNode(const ComponentNodeInfo& info)
{
    auto* node = new TreeNode(info.name);
    node->nodeId = info.id;
    node->nodeType = TreeNode::NodeType::Component;

    if (info.has_mesh) {
        TreeNode* mesh = buildMeshNode(info.mesh);
        mesh->parent = node;
        node->children.push_back(mesh);
    }
    if (info.has_geometry) {
        TreeNode* geometry = buildGeometryNode(info.geometry);
        geometry->parent = node;
        node->children.push_back(geometry);
    }

    syncSubNodes(node, false);
    return node;
}

TreeNode* TreeModel::buildModelNode(const ModelNodeInfo& info)
{
    auto* node = new TreeNode(info.name, QString::number(info.component_count));
    node->nodeId = info.id;
    node->nodeType = TreeNode::NodeType::Model;

    for (const ComponentNodeInfo& component_info : info.components) {
        TreeNode* component = buildComponentNode(component_info);
        component->parent = node;
        node->children.push_back(component);
    }

    bool any_visible = false;
    for (TreeNode* child : node->children) {
        if (child->isVisible) {
            any_visible = true;
            break;
        }
    }
    node->isVisible = any_visible || node->children.isEmpty();
    return node;
}

TreeModel::SubVisibility TreeModel::subVisibility(Index component_id) const
{
    const auto it = sub_visibility_.find(component_id);
    return it == sub_visibility_.end() ? SubVisibility { } : it->second;
}

bool TreeModel::syncSubNodes(TreeNode* node, bool notify)
{
    if (!node)
        return false;

    const int old_state = visibilityState(node);
    Index component_id = -1;
    for (TreeNode* current = node; current && current != rootNode; current = current->parent) {
        if (current->nodeType == TreeNode::NodeType::Component) {
            component_id = current->nodeId;
            break;
        }
    }
    const SubVisibility sub = subVisibility(component_id);
    bool changed = false;
    for (TreeNode* child : node->children) {
        bool visible = node->isVisible;
        if (child->nodeType == TreeNode::NodeType::Mesh)
            visible = sub.mesh;
        else if (child->nodeType == TreeNode::NodeType::Geometry)
            visible = sub.geometry;
        if (child->isVisible != visible) {
            child->isVisible = visible;
            if (notify)
                notifyNodeData(child, { IsVisibleRole, VisibilityStateRole });
            changed = true;
        }
        changed = syncSubNodes(child, notify) || changed;
    }

    // 空组件直接使用管线回传的总开关；有内容时由子节点派生。
    if (node->nodeType == TreeNode::NodeType::Component) {
        bool any_visible = false;
        for (TreeNode* child : node->children)
            any_visible = any_visible || child->isVisible;
        node->isVisible = node->children.isEmpty() ? (sub.mesh || sub.geometry) : any_visible;
    }

    // 布尔显隐可能不变而三态变化（例如只剩部分子类型可见），父行也必须刷新。
    if (visibilityState(node) != old_state) {
        if (notify)
            notifyNodeData(node, { IsVisibleRole, VisibilityStateRole });
        changed = true;
    }
    return changed;
}

int TreeModel::visibilityState(const TreeNode* node) const
{
    if (!node)
        return Qt::Unchecked;
    if (node->children.isEmpty())
        return node->isVisible ? Qt::Checked : Qt::Unchecked;

    bool any_visible = false;
    bool all_visible = true;
    for (const TreeNode* child : node->children) {
        const int state = visibilityState(child);
        any_visible = any_visible || state != Qt::Unchecked;
        all_visible = all_visible && state == Qt::Checked;
    }
    return all_visible ? Qt::Checked : any_visible ? Qt::PartiallyChecked
                                                   : Qt::Unchecked;
}

void TreeModel::notifyAncestors(TreeNode* node)
{
    for (TreeNode* current = node ? node->parent : nullptr; current && current != rootNode; current = current->parent) {
        const QModelIndex index = indexOfNode(current);
        if (index.isValid())
            emit dataChanged(index, index, { IsVisibleRole, VisibilityStateRole });
    }
}

void TreeModel::pruneStaleVisibility()
{
    if (!modelQuery_)
        return;

    for (auto it = sub_visibility_.begin(); it != sub_visibility_.end();) {
        if (!modelQuery_->hasComponent(it->first))
            it = sub_visibility_.erase(it);
        else
            ++it;
    }
}

void TreeModel::rebuildComponentLookup()
{
    component_lookup_.clear();
    for (TreeNode* model : rootNode->children) {
        for (TreeNode* component : model->children) {
            if (component->nodeType == TreeNode::NodeType::Component)
                component_lookup_[component->nodeId] = component;
        }
    }
}

QModelIndex TreeModel::indexOfNode(TreeNode* node) const
{
    if (!node || node == rootNode || !node->parent)
        return QModelIndex();

    const int row = node->parent->children.indexOf(node);
    if (row < 0)
        return QModelIndex();
    return createIndex(row, 0, node);
}

void TreeModel::notifyNodeData(TreeNode* node, const QVector<int>& roles)
{
    const QModelIndex index = indexOfNode(node);
    if (index.isValid())
        emit dataChanged(index, index, roles);
}

void TreeModel::insertChildNode(TreeNode* parent, int row, TreeNode* node)
{
    if (!parent || !node)
        return;

    const int max_row = static_cast<int>(parent->children.size());
    row = std::clamp(row, 0, max_row);
    const QModelIndex parent_index = indexOfNode(parent);

    beginInsertRows(parent_index, row, row);
    node->parent = parent;
    parent->children.insert(row, node);
    endInsertRows();
}

void TreeModel::removeChildNode(TreeNode* node)
{
    if (!node || !node->parent)
        return;

    TreeNode* parent = node->parent;
    const int row = parent->children.indexOf(node);
    if (row < 0)
        return;

    const QModelIndex parent_index = indexOfNode(parent);

    beginRemoveRows(parent_index, row, row);
    TreeNode* removed = parent->children.takeAt(row);
    removed->parent = nullptr;
    // 先让视图完成索引失效，再释放节点，避免悬垂 internalPointer。
    endRemoveRows();
    delete removed;
}

void TreeModel::moveChildNode(TreeNode* parent, int from, int to)
{
    if (!parent || from < 0 || from >= parent->children.size())
        return;

    const int max_row = static_cast<int>(parent->children.size()) - 1;
    to = std::clamp(to, 0, max_row);
    if (from == to)
        return;

    const int destination = (to > from) ? (to + 1) : to;
    const QModelIndex parent_index = indexOfNode(parent);

    if (!beginMoveRows(parent_index, from, from, parent_index, destination))
        return;

    TreeNode* node = parent->children.takeAt(from);
    parent->children.insert(to, node);
    endMoveRows();
}

void TreeModel::reconcileAllFromQuery()
{
    const QVariantList models = modelQuery_->listModels();

    std::unordered_map<Index, TreeNode*> existing_models;
    for (TreeNode* child : rootNode->children) {
        if (child->nodeType == TreeNode::NodeType::Model)
            existing_models[child->nodeId] = child;
    }

    std::vector<std::pair<Index, ModelNodeInfo>> model_infos;
    model_infos.reserve(static_cast<size_t>(models.size()));
    std::unordered_set<Index> desired_model_ids;
    for (const QVariant& value : models) {
        const Index model_id = value.toMap()["model_id"].toInt();
        desired_model_ids.insert(model_id);
        model_infos.emplace_back(model_id, queryModel(model_id));
    }

    const QVector<TreeNode*> existing = rootNode->children;
    for (TreeNode* node : existing) {
        if (node->nodeType == TreeNode::NodeType::Model && desired_model_ids.count(node->nodeId) == 0)
            removeChildNode(node);
    }

    for (int i = 0; i < static_cast<int>(model_infos.size()); ++i) {
        auto it = existing_models.find(model_infos[i].first);
        if (it == existing_models.end()) {
            insertChildNode(rootNode, i, buildModelNode(model_infos[i].second));
            continue;
        }

        TreeNode* node = it->second;
        const int row = rootNode->children.indexOf(node);
        if (row != i)
            moveChildNode(rootNode, row, i);
        syncModelNode(node, model_infos[i].second);
    }

    pruneStaleVisibility();
    rebuildComponentLookup();
}

void TreeModel::syncModelNode(TreeNode* node, const ModelNodeInfo& info)
{
    const int old_visibility = visibilityState(node);
    if (node->name != info.name) {
        node->name = info.name;
        notifyNodeData(node, { NameRole });
    }

    std::unordered_map<Index, TreeNode*> existing_components;
    for (TreeNode* child : node->children) {
        if (child->nodeType == TreeNode::NodeType::Component)
            existing_components[child->nodeId] = child;
    }

    std::unordered_set<Index> desired_component_ids;
    for (const ComponentNodeInfo& component : info.components)
        desired_component_ids.insert(component.id);

    const QVector<TreeNode*> existing = node->children;
    for (TreeNode* child : existing) {
        if (child->nodeType == TreeNode::NodeType::Component && desired_component_ids.count(child->nodeId) == 0)
            removeChildNode(child);
    }

    for (int i = 0; i < info.components.size(); ++i) {
        const ComponentNodeInfo& component_info = info.components[i];
        auto it = existing_components.find(component_info.id);
        if (it == existing_components.end()) {
            insertChildNode(node, i, buildComponentNode(component_info));
            continue;
        }

        TreeNode* component = it->second;
        const int row = node->children.indexOf(component);
        if (row != i)
            moveChildNode(node, row, i);
        syncComponentNode(component, component_info);
    }

    const QString number = QString::number(node->children.size());
    if (node->number != number) {
        node->number = number;
        notifyNodeData(node, { NumberRole });
    }
    // 空模型沿用始终可见的约定。
    if (node->children.isEmpty())
        node->isVisible = true;
    if (visibilityState(node) != old_visibility)
        notifyNodeData(node, { IsVisibleRole, VisibilityStateRole });
}

void TreeModel::syncComponentNode(TreeNode* node, const ComponentNodeInfo& info)
{
    if (node->name != info.name) {
        node->name = info.name;
        notifyNodeData(node, { NameRole });
    }

    TreeNode* mesh = findChildOfType(node, TreeNode::NodeType::Mesh);
    TreeNode* geometry = findChildOfType(node, TreeNode::NodeType::Geometry);

    if (!info.has_mesh && mesh)
        removeChildNode(mesh);
    if (!info.has_geometry && geometry)
        removeChildNode(geometry);

    mesh = findChildOfType(node, TreeNode::NodeType::Mesh);
    if (info.has_mesh) {
        if (!mesh) {
            insertChildNode(node, 0, buildMeshNode(info.mesh));
        } else {
            const int row = node->children.indexOf(mesh);
            if (row != 0)
                moveChildNode(node, row, 0);
            syncMeshNode(mesh, info.mesh);
        }
    }

    geometry = findChildOfType(node, TreeNode::NodeType::Geometry);
    if (info.has_geometry) {
        const int target_row = info.has_mesh ? 1 : 0;
        if (!geometry) {
            insertChildNode(node, target_row, buildGeometryNode(info.geometry));
        } else {
            const int row = node->children.indexOf(geometry);
            if (row != target_row)
                moveChildNode(node, row, target_row);
            syncGeometryNode(geometry, info.geometry);
        }
    }

    if (syncSubNodes(node, true))
        notifyAncestors(node);
}

void TreeModel::syncCountChildren(TreeNode* node, std::span<const CountChildSpec> specs)
{
    auto findSpec = [specs](TreeNode::NodeType type) -> const CountChildSpec* {
        for (const CountChildSpec& spec : specs) {
            if (spec.type == type)
                return &spec;
        }
        return nullptr;
    };

    const QVector<TreeNode*> existing = node->children;
    for (TreeNode* child : existing) {
        const CountChildSpec* spec = findSpec(child->nodeType);
        if (spec && spec->count <= 0)
            removeChildNode(child);
    }

    int target_row = 0;
    for (const CountChildSpec& spec : specs) {
        if (spec.count <= 0)
            continue;

        TreeNode* child = findChildOfType(node, spec.type);
        if (!child) {
            auto* created = new TreeNode(spec.name, QString::number(spec.count));
            created->nodeId = next_fake_id_--;
            created->nodeType = spec.type;
            insertChildNode(node, target_row, created);
        } else {
            const int row = node->children.indexOf(child);
            if (row != target_row)
                moveChildNode(node, row, target_row);
            const QString number = QString::number(spec.count);
            if (child->number != number) {
                child->number = number;
                notifyNodeData(child, { NumberRole });
            }
        }
        ++target_row;
    }
}

void TreeModel::syncMeshNode(TreeNode* node, const MeshNodeInfo& info)
{
    const QString number = QString::number(info.face_count + info.solid_count);
    if (node->number != number) {
        node->number = number;
        notifyNodeData(node, { NumberRole });
    }

    std::array<CountChildSpec, std::size(kMeshChildren)> specs;
    for (std::size_t i = 0; i < std::size(kMeshChildren); ++i) {
        specs[i] = { kMeshChildren[i].type, kMeshChildren[i].name,
            meshChildCount(info, kMeshChildren[i].type) };
    }
    syncCountChildren(node, specs);
    syncSubNodes(node, true);
}

void TreeModel::syncGeometryNode(TreeNode* node, const GeometryNodeInfo& info)
{
    const QString number = QString::number(info.vertex_count + info.edge_count + info.face_count + info.solid_count);
    if (node->number != number) {
        node->number = number;
        notifyNodeData(node, { NumberRole });
    }

    std::array<CountChildSpec, std::size(kGeometryChildren)> specs;
    for (std::size_t i = 0; i < std::size(kGeometryChildren); ++i) {
        specs[i] = { kGeometryChildren[i].type, kGeometryChildren[i].name,
            geometryChildCount(info, kGeometryChildren[i].type) };
    }
    syncCountChildren(node, specs);
    syncSubNodes(node, true);
}

void TreeModel::flushPendingRefresh()
{
    refresh();
    emit dataSyncFinished();
}

bool TreeModel::requestVisibility(const QModelIndex& idx, bool visible)
{
    if (!idx.isValid() || !data(idx, CanToggleVisibilityRole).toBool())
        return false;
    const auto* target = getNode(idx);
    const int component_id = data(idx, ComponentIdRole).toInt();
    const int object_id = target->nodeType == TreeNode::NodeType::Model ? target->nodeId : component_id;
    emit visibilityRequested(static_cast<int>(target->nodeType), object_id, visible);
    return true;
}

void TreeModel::requestAllVisibility(bool visible)
{
    for (int row = 0; row < rowCount(); ++row)
        requestVisibility(index(row, 0), visible);
}

void TreeModel::syncComponentVisibility(Index component_id, bool mesh_visible, bool geometry_visible)
{
    if (component_id < 0 || (modelQuery_ && !modelQuery_->hasComponent(component_id)))
        return;

    sub_visibility_[component_id] = { mesh_visible, geometry_visible };
    const auto it = component_lookup_.find(component_id);
    if (it == component_lookup_.end())
        return; // 节点尚未入树：保留管线回传，结构同步创建后再应用。

    if (syncSubNodes(it->second, true))
        notifyAncestors(it->second);
}

QModelIndex TreeModel::findIndexByNodeId(int nodeId, int depth) const
{
    std::function<QModelIndex(TreeNode*, const QModelIndex&, int)> find =
        [&](TreeNode* parent, const QModelIndex& parentIdx, int currentDepth) -> QModelIndex {
            for (int i = 0; i < parent->children.size(); ++i) {
                TreeNode* child = parent->children[i];
                QModelIndex childIdx = createIndex(i, 0, child);
                if (child->nodeId == nodeId && currentDepth == depth)
                    return childIdx;
                QModelIndex found = find(child, childIdx, currentDepth + 1);
                if (found.isValid())
                    return found;
            }
            return QModelIndex();
        };
    return find(rootNode, QModelIndex(), 0);
}

void TreeModel::setModelQuery(QObject* query)
{
    QModelQuery* next = qobject_cast<QModelQuery*>(query);
    if (next == modelQuery_)
        return;

    if (refresh_timer_)
        refresh_timer_->stop();
    refresh_pending_ = false;
    sub_visibility_.clear();
    component_lookup_.clear();

    beginResetModel();
    delete rootNode;
    rootNode = new TreeNode("root");
    rootNode->nodeId = -1;
    next_fake_id_ = -2;
    endResetModel();

    modelQuery_ = next;
    if (next)
        refresh();
}
