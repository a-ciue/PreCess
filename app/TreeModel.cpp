#include "TreeModel.h"
#include "QModelQuery.h"

#include <QVariantList>
#include <QVariantMap>

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
    case Qt::UserRole + 1:
        return node->name;

    case Qt::UserRole + 2:
        return node->number;

    case Qt::UserRole + 3:
        return !node->children.isEmpty();

    case Qt::UserRole + 4:
        return node->nodeId;

    case Qt::UserRole + 5:
        return node->isVisible;

    case Qt::UserRole + 6:
        return node->depth;

    case Qt::UserRole + 7: {
        TreeNode* cur = node;
        while (cur && cur->depth > 1)
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
    roles[Qt::UserRole + 1] = "name";
    roles[Qt::UserRole + 2] = "number";
    roles[Qt::UserRole + 3] = "hasChildren";
    roles[Qt::UserRole + 4] = "nodeId";
    roles[Qt::UserRole + 5] = "isVisible";
    roles[Qt::UserRole + 6] = "depth";
    roles[Qt::UserRole + 7] = "modelId";
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

    beginResetModel();

    delete rootNode;
    rootNode = new TreeNode("root");
    rootNode->nodeId = -1;

    QVariantList models = modelQuery_->listModels();
    for (const QVariant& mv : models) {
        QVariantMap m = mv.toMap();
        int mid = m["model_id"].toInt();
        QString mname = m["name"].toString();
        int ccount = m["component_count"].toInt();

        TreeNode* mNode = new TreeNode(mname, QString::number(ccount), rootNode, 1);
        mNode->nodeId = mid;

        QVariantList comps = modelQuery_->getComponentsSummary(mid);
        for (const QVariant& cv : comps) {
            QVariantMap c = cv.toMap();
            int cid = c["component_id"].toInt();
            QString cname = c["name"].toString();
            bool hasMesh = c["has_mesh"].toBool();
            bool hasCad = c["has_cad"].toBool();

            TreeNode* cNode = new TreeNode(cname, "", mNode, 2);
            cNode->nodeId = cid;

            if (hasMesh) {
                QVariantMap ms = modelQuery_->getMeshSummary(cid);
                int fc = ms["face_count"].toInt();
                TreeNode* meshN = new TreeNode("Mesh", QString::number(fc), cNode, 3);
                meshN->nodeId = -1;
            }

            if (hasCad) {
                QVariantMap gs = modelQuery_->getGeometrySummary(cid);
                int fc = gs["face_count"].toInt();
                TreeNode* geoN = new TreeNode("Geometry", QString::number(fc), cNode, 3);
                geoN->nodeId = -1;
            }
        }
    }

    endResetModel();
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
    emit dataChanged(idx, idx, { Qt::UserRole + 5 });
    return true;
}

void TreeModel::setModelQuery(QObject* query)
{
    modelQuery_ = qobject_cast<QModelQuery*>(query);
}
