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
        if (node->parent == rootNode) {
            for (TreeNode* child : node->children) {
                if (child->isVisible) return true;
            }
            return false;
        }
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

        QVariantList comps = modelQuery_->getComponentsSummary(mid);
        for (const QVariant& cv : comps) {
            QVariantMap c = cv.toMap();
            int cid = c["component_id"].toInt();
            QString cname = c["name"].toString();
            bool hasMesh = c["has_mesh"].toBool();
            bool hasGeometry = c["has_geometry"].toBool();

            TreeNode* cNode = new TreeNode(cname, "", mNode);
            cNode->nodeId = cid;

            if (hasMesh) {
                QVariantMap ms = modelQuery_->getMeshSummary(cid);
                int fc = ms["face_count"].toInt();
                int sc = ms["solid_count"].toInt();

                TreeNode* meshN = new TreeNode("Mesh", QString::number(fc + sc), cNode);
                meshN->nodeId = fakeId--;

                if (fc > 0) {
                    TreeNode* n2d = new TreeNode("2D", QString::number(fc), meshN);
                    n2d->nodeId = fakeId--;
                }
                if (sc > 0) {
                    TreeNode* n3d = new TreeNode("3D", QString::number(sc), meshN);
                    n3d->nodeId = fakeId--;
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

                if (vc > 0) {
                    TreeNode* vn = new TreeNode("Vertex", QString::number(vc), geoN);
                    vn->nodeId = fakeId--;
                }
                if (ec > 0) {
                    TreeNode* en = new TreeNode("Edge", QString::number(ec), geoN);
                    en->nodeId = fakeId--;
                }
                if (fc > 0) {
                    TreeNode* fn = new TreeNode("Face", QString::number(fc), geoN);
                    fn->nodeId = fakeId--;
                }
                if (sc > 0) {
                    TreeNode* sn = new TreeNode("Solid", QString::number(sc), geoN);
                    sn->nodeId = fakeId--;
                }
            }
        }
    }

    // Restore persisted visibility and prune stale entries
    std::unordered_map<int, bool> fresh;
    for (TreeNode* mNode : rootNode->children) {
        for (TreeNode* cNode : mNode->children) {
            int ckey = cNode->nodeId;
            auto cit = components_visibility_.find(ckey);
            cNode->isVisible = (cit != components_visibility_.end()) ? cit->second : true;
            fresh[ckey] = cNode->isVisible;
        }
    }
    components_visibility_ = std::move(fresh);

    for (TreeNode* mNode : rootNode->children) {
        for (TreeNode* cNode : mNode->children) {
            syncSubNodes(cNode);
        }
    }

    endResetModel();
    
    delete oldRoot;
    return true;
}

bool TreeModel::setVisibility(const QModelIndex& idx, bool visible)
{
    if (!idx.isValid()) return false;
    TreeNode* target = getNode(idx);

    if (target->parent == rootNode) {
        for (int i = 0; i < target->children.size(); ++i) {
            TreeNode* comp = target->children[i];
            comp->isVisible = visible;
            if (comp->nodeId >= 0)
                components_visibility_[comp->nodeId] = visible;
            syncSubNodes(comp);
        }
    } else {
        setNodeVisibility(target, visible);
    }

    emit dataChanged(idx, idx, { IsVisibleRole });
    emitDescendantDataChanged(idx);

    QModelIndex parentIdx = parent(idx);
    if (parentIdx.isValid())
        emit dataChanged(parentIdx, parentIdx, { IsVisibleRole });

    return true;
}

void TreeModel::setNodeVisibility(TreeNode* node, bool visible)
{
    node->isVisible = visible;
    if (node->nodeId >= 0)
        components_visibility_[node->nodeId] = visible;
    for (TreeNode* child : node->children)
        setNodeVisibility(child, visible);
}

void TreeModel::syncSubNodes(TreeNode* node)
{
    for (TreeNode* child : node->children) {
        child->isVisible = node->isVisible;
        syncSubNodes(child);
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
