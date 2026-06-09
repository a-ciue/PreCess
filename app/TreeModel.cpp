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

    case IsVisibleRole:
        return node->isVisible;

    case ModelIdRole: {
        TreeNode* cur = node;
        while (cur && cur->parent && cur->parent != rootNode)
            cur = cur->parent;
        return cur ? cur->nodeId : -1;
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
    roles[ModelIdRole]  = "modelId";
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
            bool hasCad = c["has_cad"].toBool();

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

            if (hasCad) {
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

    endResetModel();
    delete oldRoot;
    return true;
}

bool TreeModel::setVisibility(int row, const QModelIndex& parentIndex, bool visible)
{
    TreeNode* parentNode = getNode(parentIndex);

    if (row < 0 || row >= parentNode->children.size())
        return false;

    TreeNode* target = parentNode->children[row];
    target->isVisible = visible;

    QModelIndex idx = createIndex(row, 0, target);
    emit dataChanged(idx, idx, { IsVisibleRole });
    return true;
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
