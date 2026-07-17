#include "TreeModel.h"
#include "QModelQuery.h"

#include <QVariantList>
#include <QVariantMap>
#include <functional>

TreeModel::TreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{
    rootNode = new TreeNode("root");
    rootNode->nodeId = -1;
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

    auto oldRoot = rootNode;
    rootNode = new TreeNode("root");
    rootNode->nodeId = -1;

    beginResetModel();

    QVariantList models = modelQuery_->listModels();
    int fakeId = -2;
    for (const QVariant& mv : models) {
        QVariantMap m = mv.toMap();
        int mid = m["model_id"].toInt();
        QString mname = m["name"].toString();
        int ccount = m["component_count"].toInt();

        TreeNode* mNode = new TreeNode(mname, QString::number(ccount), rootNode);
        mNode->nodeId = mid;
        mNode->nodeType = TreeNode::NodeType::Model;

        QVariantList comps = modelQuery_->getComponentsSummary(mid);
        for (const QVariant& cv : comps) {
            QVariantMap c = cv.toMap();
            int cid = c["component_id"].toInt();
            QString cname = c["name"].toString();
            bool hasMesh = c["has_mesh"].toBool();
            bool hasGeometry = c["has_geometry"].toBool();

            TreeNode* cNode = new TreeNode(cname, "", mNode);
            cNode->nodeId = cid;
            cNode->nodeType = TreeNode::NodeType::Component;

            if (hasMesh) {
                QVariantMap ms = modelQuery_->getMeshSummary(cid);
                int fc = ms["face_count"].toInt();
                int sc = ms["solid_count"].toInt();

                TreeNode* meshN = new TreeNode("Mesh", QString::number(fc + sc), cNode);
                meshN->nodeId = fakeId--;
                meshN->nodeType = TreeNode::NodeType::Mesh;

                if (fc > 0) {
                    TreeNode* n2d = new TreeNode("2D", QString::number(fc), meshN);
                    n2d->nodeId = fakeId--;
                    n2d->nodeType = TreeNode::NodeType::D2;
                }
                if (sc > 0) {
                    TreeNode* n3d = new TreeNode("3D", QString::number(sc), meshN);
                    n3d->nodeId = fakeId--;
                    n3d->nodeType = TreeNode::NodeType::D3;
                }
            }

            if (hasGeometry) {
                QVariantMap gs = modelQuery_->getGeometrySummary(cid);
                int vc = gs["vertex_count"].toInt();
                int ec = gs["edge_count"].toInt();
                int fc = gs["face_count"].toInt();
                int sc = gs["solid_count"].toInt();

                TreeNode* geoN = new TreeNode("Geometry", QString::number(vc + ec + fc + sc), cNode);
                geoN->nodeId = fakeId--;
                geoN->nodeType = TreeNode::NodeType::Geometry;

                if (vc > 0) {
                    TreeNode* vn = new TreeNode("Vertex", QString::number(vc), geoN);
                    vn->nodeId = fakeId--;
                    vn->nodeType = TreeNode::NodeType::Vertex;
                }
                if (ec > 0) {
                    TreeNode* en = new TreeNode("Edge", QString::number(ec), geoN);
                    en->nodeId = fakeId--;
                    en->nodeType = TreeNode::NodeType::Edge;
                }
                if (fc > 0) {
                    TreeNode* fn = new TreeNode("Face", QString::number(fc), geoN);
                    fn->nodeId = fakeId--;
                    fn->nodeType = TreeNode::NodeType::Face;
                }
                if (sc > 0) {
                    TreeNode* sn = new TreeNode("Solid", QString::number(sc), geoN);
                    sn->nodeId = fakeId--;
                    sn->nodeType = TreeNode::NodeType::Solid;
                }
            }
        }
    }

    // Restore persisted visibility and prune stale entries
    std::unordered_map<int, bool> freshComp;
    std::unordered_map<int, bool> freshMesh;
    std::unordered_map<int, bool> freshGeom;
    for (TreeNode* mNode : rootNode->children) {
        for (TreeNode* cNode : mNode->children) {
            int ckey = cNode->nodeId;
            auto cit = components_visibility_.find(ckey);
            cNode->isVisible = (cit != components_visibility_.end()) ? cit->second : true;
            freshComp[ckey] = cNode->isVisible;

            // Restore mesh visibility
            for (TreeNode* child : cNode->children) {
                if (child->nodeType == TreeNode::NodeType::Mesh) {
                    auto mit = mesh_visibility_.find(ckey);
                    child->isVisible = (mit != mesh_visibility_.end()) ? mit->second : true;
                    freshMesh[ckey] = child->isVisible;
                } else if (child->nodeType == TreeNode::NodeType::Geometry) {
                    auto git = geometry_visibility_.find(ckey);
                    child->isVisible = (git != geometry_visibility_.end()) ? git->second : true;
                    freshGeom[ckey] = child->isVisible;
                }
            }
        }
    }
    components_visibility_ = std::move(freshComp);
    mesh_visibility_ = std::move(freshMesh);
    geometry_visibility_ = std::move(freshGeom);

    for (TreeNode* mNode : rootNode->children) {
        for (TreeNode* cNode : mNode->children) {
            syncSubNodes(cNode);
        }
    }
    for (TreeNode* mNode : rootNode->children) {
        for (TreeNode* cNode : mNode->children) {
            for (TreeNode* child : cNode->children) {
                if (child->nodeType == TreeNode::NodeType::Mesh || child->nodeType == TreeNode::NodeType::Geometry)
                    propagateUp(child);
            }
        }
    }

    endResetModel();
    
    delete oldRoot;
    return true;
}

void TreeModel::setComponentVisibility(TreeNode* comp, bool visible)
{
    comp->isVisible = visible;
    if (comp->nodeId >= 0) {
        components_visibility_[comp->nodeId] = visible;
        if (visible) {
            mesh_visibility_[comp->nodeId] = true;
            geometry_visibility_[comp->nodeId] = true;
        }
    }
    syncSubNodes(comp);
}

bool TreeModel::setVisibility(const QModelIndex& idx, bool visible)
{
    if (!idx.isValid()) return false;
    TreeNode* target = getNode(idx);

    if (target->parent == rootNode) {
        for (int i = 0; i < target->children.size(); ++i) {
            TreeNode* comp = target->children[i];
            setComponentVisibility(comp, visible);
        }
        if (!target->children.isEmpty())
            propagateUp(target->children.first());
    } else if (target->nodeType == TreeNode::NodeType::Mesh && target->parent) {
        int compId = target->parent->nodeId;
        mesh_visibility_[compId] = visible;
        target->isVisible = visible;
        if (visible) {
            target->parent->isVisible = true;
            components_visibility_[compId] = true;
        }
        syncSubNodes(target->parent);
        propagateUp(target);
    } else if (target->nodeType == TreeNode::NodeType::Geometry && target->parent) {
        int compId = target->parent->nodeId;
        geometry_visibility_[compId] = visible;
        target->isVisible = visible;
        if (visible) {
            target->parent->isVisible = true;
            components_visibility_[compId] = true;
        }
        syncSubNodes(target->parent);
        propagateUp(target);
    } else {
        setComponentVisibility(target, visible);
        propagateUp(target);
    }

    emit dataChanged(idx, idx, { IsVisibleRole });
    emitDescendantDataChanged(idx);

    QModelIndex parentIdx = parent(idx);
    if (parentIdx.isValid())
        emit dataChanged(parentIdx, parentIdx, { IsVisibleRole });
    QModelIndex grandParentIdx = parentIdx.isValid() ? parent(parentIdx) : QModelIndex();
    if (grandParentIdx.isValid())
        emit dataChanged(grandParentIdx, grandParentIdx, { IsVisibleRole });

    return true;
}

void TreeModel::setAllVisibility(bool visible)
{
    for (TreeNode* mNode : rootNode->children) {
        for (TreeNode* cNode : mNode->children) {
            setComponentVisibility(cNode, visible);
            if (!visible) {
                mesh_visibility_[cNode->nodeId] = false;
                geometry_visibility_[cNode->nodeId] = false;
            }
        }
    }
    QModelIndex topLeft  = index(0, 0);
    QModelIndex botRight = index(rowCount() - 1, 0);
    emit dataChanged(topLeft, botRight, { IsVisibleRole });
    for (int r = 0; r < rowCount(); ++r)
        emitDescendantDataChanged(index(r, 0, QModelIndex()));
}

void TreeModel::syncSubNodes(TreeNode* node)
{
    for (TreeNode* child : node->children) {
        if (child->nodeType == TreeNode::NodeType::Mesh) {
            child->isVisible = node->isVisible && mesh_visibility_[node->nodeId];
        } else if (child->nodeType == TreeNode::NodeType::Geometry) {
            child->isVisible = node->isVisible && geometry_visibility_[node->nodeId];
        } else {
            child->isVisible = node->isVisible;
        }
        syncSubNodes(child);
    }
}

void TreeModel::propagateUp(TreeNode* node) {
    TreeNode* cur = node->parent;
    while (cur && cur != rootNode) {
        bool anyVisible = false;
        for (TreeNode* child : cur->children) {
            if (child->isVisible) {
                anyVisible = true;
                break;
            }
        }
        cur->isVisible = anyVisible || cur->children.isEmpty();
        cur = cur->parent;
    }
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
    modelQuery_ = qobject_cast<QModelQuery*>(query);
}
