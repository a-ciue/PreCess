#pragma once

#include <QAbstractItemModel>
#include <QVariant>
#include <QVector>
#include <QtQml/qqmlregistration.h>
#include <unordered_map>

#include "QModelQuery.h"

struct TreeNode {
    QString name;
    QString number;
    int nodeId = -1;
    bool isVisible = true;
    QVector<TreeNode*> children;
    TreeNode* parent = nullptr;

    TreeNode(const QString& n, const QString& num = "", TreeNode* p = nullptr)
        : name(n), number(num), parent(p)
    {
        if (p)
            p->children << this;
    }

    ~TreeNode() { qDeleteAll(children); }
};

class TreeModel : public QAbstractItemModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(TreeModel)
    Q_PROPERTY(QObject* modelQuery READ getModelQuery WRITE setModelQuery REQUIRED)

public:
    enum TreeRole {
        NameRole    = Qt::UserRole + 1,
        NumberRole,
        NodeIdRole,
        IsVisibleRole,
        ComponentIdRole
    };
    Q_ENUM(TreeRole)
    explicit TreeModel(QObject* parent = nullptr);
    ~TreeModel();

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE bool refresh();
    Q_INVOKABLE bool setVisibility(const QModelIndex& idx, bool visible);
    Q_INVOKABLE QModelIndex findIndexByNodeId(int nodeId, int depth) const;
    Q_INVOKABLE void setAllVisibility(bool visible);

    QObject* getModelQuery() const { return modelQuery_; }
    void setModelQuery(QObject* query);

private:
    TreeNode* getNode(const QModelIndex& index) const;
    void emitDescendantDataChanged(const QModelIndex& parentIndex);
    void setNodeVisibility(TreeNode* node, bool visible);
    void setComponentVisibility(TreeNode* node, bool visible);
    void syncSubNodes(TreeNode* node);

    TreeNode* rootNode = nullptr;
    QModelQuery* modelQuery_ = nullptr;
    std::unordered_map<int, bool> components_visibility_;
};
