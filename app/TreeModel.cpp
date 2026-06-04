#include "TreeModel.h"

TreeModel::TreeModel(QObject* parent)
    : QAbstractItemModel(parent)
{
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

    return createIndex(row, column,
        parentNode->children[row]);
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
    //此处规定列数
    return 1;
}

QVariant TreeModel::data(const QModelIndex& index,int role) const
{
    if (!index.isValid())
        return QVariant();

    TreeNode* node = static_cast<TreeNode*>(index.internalPointer());

    switch (role) {
    case Qt::DisplayRole:
        return node->name;

    case Qt::UserRole + 1:
        return node->name;

    case Qt::UserRole + 2:
        return node->number;

    case Qt::UserRole + 3:
        return !node->children.isEmpty();

    default:
        return QVariant();
    }
}

QHash<int, QByteArray> TreeModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Qt::UserRole + 1] = "name";
    roles[Qt::UserRole + 2] = "number";
    return roles;
}

TreeNode* TreeModel::getNode(const QModelIndex& index) const
{
    if (index.isValid())
        return static_cast<TreeNode*>(index.internalPointer());

    return rootNode;
}

bool TreeModel::removeNode(int row, const QModelIndex& parentIndex)
{
    TreeNode* parentNode = getNode(parentIndex);

    if (row < 0 || row >= parentNode->children.size())
        return false;

    

    return true;
}

// 刷新机制,调用此函数后会重建树,并刷新界面
bool TreeModel::rebuiltTree()
{
    beginResetModel();

    //树删除
    delete rootNode;
    //树重建


    endResetModel();

    return true;
}