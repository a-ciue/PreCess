#pragma once

#include <QAbstractItemModel>
#include <QVariant>
#include <QVector>
#include <QtQml/qqmlregistration.h>

struct TreeNode {
    QString name;
    QString value;
    QVector<TreeNode*> children;
    TreeNode* parent;

    TreeNode(const QString& n, const QString& v = "", TreeNode* p = nullptr): name(n), value(v), parent(p)
    {
        if (p)
            p->children << this;
    }

    ~TreeNode() { qDeleteAll(children); }
};

class TreeModel : public QAbstractItemModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(TreeModel)

public:
    explicit TreeModel(QObject* parent = nullptr);
    ~TreeModel();

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;

    QModelIndex parent(const QModelIndex& child) const override;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    int columnCount(const QModelIndex& parent = QModelIndex()) const override;

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE bool removeNode(int row, const QModelIndex& parentIndex);

    Q_INVOKABLE bool rebuiltTree();

private:
    TreeNode* getNode(const QModelIndex& index) const;

private:
    TreeNode* rootNode;
};