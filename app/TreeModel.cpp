#include "TreeModel.h"
#include "QModelQuery.h"

#include <QTimer>
#include <spdlog/spdlog.h>
#include <QVariantList>
#include <QVariantMap>
#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
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

    data_sync_timer_ = new QTimer(this);
    data_sync_timer_->setSingleShot(true);
    data_sync_timer_->setInterval(50);
    connect(data_sync_timer_, &QTimer::timeout, this, &TreeModel::flushPendingDataSync);
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

    case IsVisibleRole: {
        if (node == rootNode) return true;
        for (TreeNode* child : node->children) {
            if (child->isVisible) return true;
        }
        if (!node->children.isEmpty())
            return false;
        return node->isVisible;
    }

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

    if (!initialized_) {
        beginResetModel();
        rebuildAllFromQuery();
        initialized_ = true;
        endResetModel();
        return true;
    }

    if (reconcileAllFromQuery())
        return true;

    spdlog::warn("TreeModel: incremental reconcile failed, fallback to full reset");
    beginResetModel();
    rebuildAllFromQuery();
    initialized_ = true;
    endResetModel();
    return true;
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

    applyComponentVisibilityState(node, false);
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

bool TreeModel::isComponentEnabled(Index component_id) const
{
    const auto it = components_visibility_.find(component_id);
    return it == components_visibility_.end() || it->second;
}

void TreeModel::setSubtreeVisibility(TreeNode* node, bool visible, bool notify)
{
    if (!node)
        return;

    if (node->isVisible != visible) {
        node->isVisible = visible;
        if (notify)
            notifyNodeData(node, { IsVisibleRole });
    }
    for (TreeNode* child : node->children)
        setSubtreeVisibility(child, visible, notify);
}

void TreeModel::applyComponentVisibilityState(TreeNode* component_node, bool notify)
{
    if (!component_node || component_node->nodeType != TreeNode::NodeType::Component)
        return;

    const bool enabled = isComponentEnabled(component_node->nodeId);
    const auto sub = sub_visibility_.find(component_node->nodeId);
    const bool mesh_visible = enabled && (sub == sub_visibility_.end() || sub->second.mesh.value_or(true));
    const bool geometry_visible = enabled && (sub == sub_visibility_.end() || sub->second.geometry.value_or(true));

    bool any_visible = false;
    for (TreeNode* child : component_node->children) {
        bool visible = enabled;
        if (child->nodeType == TreeNode::NodeType::Mesh)
            visible = mesh_visible;
        else if (child->nodeType == TreeNode::NodeType::Geometry)
            visible = geometry_visible;
        setSubtreeVisibility(child, visible, notify);
        any_visible = any_visible || visible;
    }

    // 空组件直接使用组件级设置；有内容的组件由子节点汇总，避免再用派生结果反推设置。
    const bool visible = component_node->children.isEmpty() ? enabled : any_visible;
    if (component_node->isVisible != visible) {
        component_node->isVisible = visible;
        if (notify)
            notifyNodeData(component_node, { IsVisibleRole });
    }

    recomputeVisibility(component_node->parent, notify);
}

void TreeModel::eraseComponentVisibility(Index component_id)
{
    components_visibility_.erase(component_id);
    sub_visibility_.erase(component_id);
}

void TreeModel::pruneStaleVisibility()
{
    if (!modelQuery_)
        return;

    for (auto it = components_visibility_.begin(); it != components_visibility_.end();) {
        if (!modelQuery_->hasComponent(it->first))
            it = components_visibility_.erase(it);
        else
            ++it;
    }
    for (auto it = sub_visibility_.begin(); it != sub_visibility_.end();) {
        if (!modelQuery_->hasComponent(it->first))
            it = sub_visibility_.erase(it);
        else
            ++it;
    }
}

void TreeModel::registerSubtree(TreeNode* node)
{
    if (!node)
        return;

    if (node->nodeType == TreeNode::NodeType::Model)
        model_nodes_[node->nodeId] = node;
    else if (node->nodeType == TreeNode::NodeType::Component)
        component_nodes_[node->nodeId] = node;

    for (TreeNode* child : node->children)
        registerSubtree(child);
}

void TreeModel::unregisterSubtree(TreeNode* node)
{
    if (!node)
        return;

    if (node->nodeType == TreeNode::NodeType::Model) {
        model_nodes_.erase(node->nodeId);
        pending_model_ids_.erase(node->nodeId);
    } else if (node->nodeType == TreeNode::NodeType::Component) {
        component_nodes_.erase(node->nodeId);
    }

    for (TreeNode* child : node->children)
        unregisterSubtree(child);
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

void TreeModel::insertModelNode(Index model_id, const ModelNodeInfo& info)
{
    const QVariantList models = modelQuery_->listModels();
    int row = 0;
    bool found = false;
    for (const QVariant& value : models) {
        const Index current_id = value.toMap()["model_id"].toInt();
        if (current_id == model_id) {
            found = true;
            break;
        }
        if (model_nodes_.find(current_id) != model_nodes_.end())
            ++row;
    }
    if (!found) {
        spdlog::warn("TreeModel: model {} missing from model list", model_id);
        return;
    }

    insertChildNode(rootNode, row, buildModelNode(info));
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
    registerSubtree(node);
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
    unregisterSubtree(node);
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

void TreeModel::recomputeVisibility(TreeNode* node, bool notify)
{
    for (TreeNode* current = node; current && current != rootNode; current = current->parent) {
        // 叶子节点的显隐来自已保存的设置，不能被汇总规则改写；空模型沿用始终可见的约定。
        if (current->children.isEmpty()) {
            if (current->nodeType == TreeNode::NodeType::Model && !current->isVisible) {
                current->isVisible = true;
                if (notify)
                    notifyNodeData(current, { IsVisibleRole });
            }
            continue;
        }

        bool any_visible = false;
        for (TreeNode* child : current->children) {
            if (child->isVisible) {
                any_visible = true;
                break;
            }
        }

        if (current->isVisible != any_visible) {
            current->isVisible = any_visible;
            if (notify)
                notifyNodeData(current, { IsVisibleRole });
        }
    }
}

void TreeModel::updateAncestorVisibility(TreeNode* node)
{
    recomputeVisibility(node, true);
}

void TreeModel::rebuildAllFromQuery()
{
    delete rootNode;
    rootNode = new TreeNode("root");
    rootNode->nodeId = -1;

    model_nodes_.clear();
    component_nodes_.clear();
    next_fake_id_ = -2;
    pending_model_ids_.clear();
    if (data_sync_timer_)
        data_sync_timer_->stop();

    const QVariantList models = modelQuery_->listModels();
    for (const QVariant& value : models) {
        const QVariantMap model = value.toMap();
        TreeNode* node = buildModelNode(queryModel(model["model_id"].toInt()));
        node->parent = rootNode;
        rootNode->children.push_back(node);
        registerSubtree(node);
    }

    pruneStaleVisibility();
}

bool TreeModel::reconcileAllFromQuery()
{
    const QVariantList models = modelQuery_->listModels();

    std::vector<std::pair<Index, ModelNodeInfo>> model_infos;
    model_infos.reserve(static_cast<size_t>(models.size()));
    std::unordered_set<Index> desired_model_ids;
    for (const QVariant& value : models) {
        const Index model_id = value.toMap()["model_id"].toInt();
        desired_model_ids.insert(model_id);
        model_infos.emplace_back(model_id, queryModel(model_id));
    }

    const QVector<TreeNode*> existing_models = rootNode->children;
    for (TreeNode* node : existing_models) {
        if (node->nodeType == TreeNode::NodeType::Model && desired_model_ids.count(node->nodeId) == 0)
            removeChildNode(node);
    }

    for (int i = 0; i < static_cast<int>(model_infos.size()); ++i) {
        auto it = model_nodes_.find(model_infos[i].first);
        if (it == model_nodes_.end()) {
            insertChildNode(rootNode, i, buildModelNode(model_infos[i].second));
            continue;
        }

        TreeNode* node = it->second;
        const int row = rootNode->children.indexOf(node);
        if (row != i)
            moveChildNode(rootNode, row, i);
        if (!syncModelNode(node, model_infos[i].second))
            return false;
    }

    pruneStaleVisibility();
    return true;
}

bool TreeModel::syncModelNode(TreeNode* node, const ModelNodeInfo& info)
{
    if (node->name != info.name) {
        node->name = info.name;
        notifyNodeData(node, { NameRole });
    }

    std::unordered_set<Index> desired_component_ids;
    for (const ComponentNodeInfo& component : info.components)
        desired_component_ids.insert(component.id);

    const QVector<TreeNode*> existing_components = node->children;
    for (TreeNode* child : existing_components) {
        if (child->nodeType == TreeNode::NodeType::Component && desired_component_ids.count(child->nodeId) == 0)
            removeChildNode(child);
    }

    for (int i = 0; i < info.components.size(); ++i) {
        const ComponentNodeInfo& component_info = info.components[i];
        auto it = component_nodes_.find(component_info.id);
        if (it == component_nodes_.end()) {
            insertChildNode(node, i, buildComponentNode(component_info));
            continue;
        }

        TreeNode* component = it->second;
        if (component->parent != node)
            return false;

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
    updateAncestorVisibility(node);
    return true;
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

    // 真实删除子类型时清空对应设置（与"尚未入树"区分）：重建后默认可见。
    auto& sub = sub_visibility_[node->nodeId];
    if (!info.has_mesh)
        sub.mesh.reset();
    if (!info.has_geometry)
        sub.geometry.reset();

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

    applyComponentVisibilityState(node, true);
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
}

void TreeModel::syncModelAdded(Index model_id)
{
    if (!initialized_) {
        refresh();
        return;
    }
    if (!modelQuery_->hasModel(model_id))
        return;

    ModelNodeInfo info = queryModel(model_id);
    auto existing = model_nodes_.find(model_id);
    if (existing != model_nodes_.end()) {
        if (!syncModelNode(existing->second, info))
            refresh();
        return;
    }

    insertModelNode(model_id, info);
}

void TreeModel::syncModelRemoved(Index model_id)
{
    if (!initialized_) {
        refresh();
        return;
    }

    auto it = model_nodes_.find(model_id);
    if (it != model_nodes_.end())
        removeChildNode(it->second);
    pruneStaleVisibility();
}

void TreeModel::syncModelChanged(Index model_id)
{
    if (!initialized_) {
        refresh();
        return;
    }

    if (!modelQuery_->hasModel(model_id)) {
        syncModelRemoved(model_id);
        return;
    }

    ModelNodeInfo info = queryModel(model_id);
    auto it = model_nodes_.find(model_id);
    if (it == model_nodes_.end()) {
        insertModelNode(model_id, info);
        return;
    }

    if (!syncModelNode(it->second, info))
        refresh();
}

void TreeModel::syncComponentRemoved(Index component_id)
{
    if (!initialized_) {
        refresh();
        return;
    }

    auto it = component_nodes_.find(component_id);
    if (it == component_nodes_.end()) {
        if (!modelQuery_->hasComponent(component_id))
            eraseComponentVisibility(component_id);
        return;
    }

    const bool component_exists = modelQuery_->hasComponent(component_id);
    TreeNode* component = it->second;
    TreeNode* model = component->parent;
    removeChildNode(component);
    if (!component_exists)
        eraseComponentVisibility(component_id);

    if (model && model->nodeType == TreeNode::NodeType::Model) {
        const QString number = QString::number(model->children.size());
        if (model->number != number) {
            model->number = number;
            notifyNodeData(model, { NumberRole });
        }
        updateAncestorVisibility(model);
    }
}

void TreeModel::queueModelChanged(Index model_id)
{
    if (!initialized_) {
        refresh();
        return;
    }
    pending_model_ids_.insert(model_id);
    if (!data_sync_timer_->isActive())
        data_sync_timer_->start();
}

void TreeModel::queueComponentChanged(Index component_id)
{
    if (!initialized_) {
        refresh();
        return;
    }

    const Index model_id = modelQuery_->findModelIdByComponent(component_id);
    if (model_id < 0)
        return;

    queueModelChanged(model_id);
}

void TreeModel::flushPendingDataSync()
{
    const auto model_ids = std::exchange(pending_model_ids_, {});

    for (Index model_id : model_ids)
        syncModelChanged(model_id);

    // 处理期间经信号回调新排队的 id 留到下一轮定时器消费，避免递归 flush。
    if (!pending_model_ids_.empty() && data_sync_timer_ && !data_sync_timer_->isActive())
        data_sync_timer_->start();

    emit dataSyncFinished();
}

void TreeModel::setComponentVisibility(TreeNode* comp, bool visible)
{
    if (!comp || comp->nodeId < 0)
        return;

    components_visibility_[comp->nodeId] = visible;
    if (visible) {
        // 显示组件沿用既有语义：同时恢复其全部子类型。
        auto& sub = sub_visibility_[comp->nodeId];
        sub.mesh = true;
        sub.geometry = true;
    }
    applyComponentVisibilityState(comp, true);
}

bool TreeModel::setVisibility(const QModelIndex& idx, bool visible)
{
    if (!idx.isValid())
        return false;
    TreeNode* target = getNode(idx);

    if (target->parent == rootNode) {
        for (TreeNode* comp : target->children)
            setComponentVisibility(comp, visible);
        updateAncestorVisibility(target);
    } else if ((target->nodeType == TreeNode::NodeType::Mesh
                   || target->nodeType == TreeNode::NodeType::Geometry)
        && target->parent && target->parent->nodeType == TreeNode::NodeType::Component) {
        TreeNode* comp = target->parent;
        auto& sub = sub_visibility_[comp->nodeId];
        if (target->nodeType == TreeNode::NodeType::Mesh)
            sub.mesh = visible;
        else
            sub.geometry = visible;
        if (visible)
            components_visibility_[comp->nodeId] = true;
        applyComponentVisibilityState(comp, true);
    } else {
        setComponentVisibility(target, visible);
    }

    return true;
}

void TreeModel::applyComponentVisibility(Index component_id, bool mesh_visible, bool geometry_visible)
{
    if (!modelQuery_ || !modelQuery_->hasComponent(component_id))
        return;

    components_visibility_[component_id] = mesh_visible || geometry_visible;
    auto& sub = sub_visibility_[component_id];
    sub.mesh = mesh_visible;
    sub.geometry = geometry_visible;

    const auto it = component_nodes_.find(component_id);
    if (it == component_nodes_.end())
        return; // 节点尚未入树：保留结果，结构同步创建后再应用。

    applyComponentVisibilityState(it->second, true);
}

void TreeModel::setAllVisibility(bool visible)
{
    for (TreeNode* mNode : rootNode->children) {
        for (TreeNode* cNode : mNode->children) {
            if (cNode->nodeType != TreeNode::NodeType::Component)
                continue;
            components_visibility_[cNode->nodeId] = visible;
            auto& sub = sub_visibility_[cNode->nodeId];
            sub.mesh = visible;
            sub.geometry = visible;
            applyComponentVisibilityState(cNode, false);
        }
        recomputeVisibility(mNode, false);
    }
    QModelIndex topLeft  = index(0, 0);
    QModelIndex botRight = index(rowCount() - 1, 0);
    emit dataChanged(topLeft, botRight, { IsVisibleRole });
    for (int r = 0; r < rowCount(); ++r)
        emitDescendantDataChanged(index(r, 0, QModelIndex()));
}

void TreeModel::emitDescendantDataChanged(const QModelIndex& parentIndex)
{
    int count = rowCount(parentIndex);
    for (int i = 0; i < count; ++i) {
        QModelIndex child = index(i, 0, parentIndex);
        emit dataChanged(child, child, { IsVisibleRole });
        emitDescendantDataChanged(child);
    }
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

    const bool was_initialized = initialized_;
    modelQuery_ = next;
    initialized_ = false;
    pending_model_ids_.clear();
    if (data_sync_timer_)
        data_sync_timer_->stop();
    components_visibility_.clear();
    sub_visibility_.clear();

    if (!was_initialized)
        return;

    if (next) {
        refresh();
        return;
    }

    beginResetModel();
    delete rootNode;
    rootNode = new TreeNode("root");
    rootNode->nodeId = -1;
    model_nodes_.clear();
    component_nodes_.clear();
    endResetModel();
}
