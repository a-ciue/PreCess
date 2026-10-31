#pragma once

#include <QAbstractItemModel>
#include <QVariant>
#include <QVector>
#include <QtQml/qqmlregistration.h>
#include <span>
#include <unordered_map>
#include <unordered_set>

#include "QModelQuery.h"

class QTimer;

struct TreeNode {
    enum class NodeType {
        Model, Component, Mesh, Geometry,
        D2, D3,
        Vertex, Edge, Face, Solid
    };

    QString name;
    QString number;
    int nodeId = -1;
    NodeType nodeType = NodeType::Component;
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
        ComponentIdRole,
        NodeTypeRole
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

    /**
     * @brief 增量同步：模型新增（结构事件，立即执行）
     * @note 模型层已先完成数据写入；本方法只读取查询结果并插入对应行。
     */
    Q_INVOKABLE void syncModelAdded(Index model_id);
    //! @brief 增量同步：模型移除（结构事件，立即执行）
    Q_INVOKABLE void syncModelRemoved(Index model_id);
    /**
     * @brief 增量同步：模型数据变化（数据事件；QML 侧经 queueModelChanged 合并）
     * @note 模型层当前无内部发射点，保留给 ModelOperator::notifyChanged 的 SDK 调用方。
     */
    Q_INVOKABLE void syncModelChanged(Index model_id);
    /**
     * @brief 增量同步：模型名称变化（仅 dataChanged）
     * @note 模型层当前无内部改名发射点，保留给后续模型层事件与 SDK 调用方。
     */
    Q_INVOKABLE void syncModelNameChanged(Index model_id, const QString& name);
    //! @brief 增量同步：组件新增或内容变化（新增组件时立即插入，内容变化由调用方决定时机）
    Q_INVOKABLE void syncComponentChanged(Index component_id);
    //! @brief 增量同步：组件移除（结构事件，立即执行）
    Q_INVOKABLE void syncComponentRemoved(Index component_id);

    //! @brief 数据事件合并入口：50ms 内同一模型只同步一次
    Q_INVOKABLE void queueModelChanged(Index model_id);
    /**
     * @brief 数据事件合并入口：组件已存在时 50ms 合并，尚未入树时立即插入
     * @note componentChanged 同时承担“新增组件”通知，因此不能一律延迟。
     */
    Q_INVOKABLE void queueComponentChanged(Index component_id);

    QObject* getModelQuery() const { return modelQuery_; }
    void setModelQuery(QObject* query);

signals:
    //! @brief 数据事件合并同步完成
    void dataSyncFinished();

private:
    struct MeshNodeInfo {
        int face_count = 0;
        int solid_count = 0;
    };
    struct GeometryNodeInfo {
        int vertex_count = 0;
        int edge_count = 0;
        int face_count = 0;
        int solid_count = 0;
    };
    struct ComponentNodeInfo {
        Index id = -1;
        QString name;
        bool has_mesh = false;
        bool has_geometry = false;
        MeshNodeInfo mesh;
        GeometryNodeInfo geometry;
    };
    struct ModelNodeInfo {
        Index id = -1;
        QString name;
        int component_count = 0;
        QVector<ComponentNodeInfo> components;
    };

    static int meshChildCount(const MeshNodeInfo& info, TreeNode::NodeType type);
    static int geometryChildCount(const GeometryNodeInfo& info, TreeNode::NodeType type);

    TreeNode* getNode(const QModelIndex& index) const;
    void emitDescendantDataChanged(const QModelIndex& parentIndex);
    void setComponentVisibility(TreeNode* node, bool visible);
    void syncSubNodes(TreeNode* node);
    void propagateUp(TreeNode* node);

    ModelNodeInfo queryModel(Index model_id) const;
    ComponentNodeInfo queryComponent(Index component_id) const;
    void fillComponentData(ComponentNodeInfo& info) const;

    TreeNode* buildModelNode(const ModelNodeInfo& info);
    TreeNode* buildComponentNode(const ComponentNodeInfo& info);
    TreeNode* buildMeshNode(const MeshNodeInfo& info);
    TreeNode* buildGeometryNode(const GeometryNodeInfo& info);

    bool syncModelNode(TreeNode* node, const ModelNodeInfo& info);
    void syncComponentNode(TreeNode* node, const ComponentNodeInfo& info);
    void syncMeshNode(TreeNode* node, const MeshNodeInfo& info);
    void syncGeometryNode(TreeNode* node, const GeometryNodeInfo& info);

    struct CountChildSpec {
        TreeNode::NodeType type;
        const char* name;
        int count = 0;
    };
    void syncCountChildren(TreeNode* node, std::span<const CountChildSpec> specs);

    void insertModelNode(Index model_id, const ModelNodeInfo& info);
    void insertChildNode(TreeNode* parent, int row, TreeNode* node);
    void removeChildNode(TreeNode* node);
    void moveChildNode(TreeNode* parent, int from, int to);
    QModelIndex indexOfNode(TreeNode* node) const;
    void notifyNodeData(TreeNode* node, const QVector<int>& roles);

    void registerSubtree(TreeNode* node);
    void unregisterSubtree(TreeNode* node);
    void applyStoredVisibility(TreeNode* component_node);
    void ensureSubVisibilityEntry(TreeNode* component_node, const ComponentNodeInfo& info);
    void eraseComponentVisibility(Index component_id);
    void pruneStaleVisibility();
    void recomputeVisibility(TreeNode* node, bool notify);
    void updateAncestorVisibility(TreeNode* node);
    void rebuildAllFromQuery();
    bool reconcileAllFromQuery();
    void flushPendingDataSync();

    TreeNode* rootNode = nullptr;
    QModelQuery* modelQuery_ = nullptr;
    std::unordered_map<Index, TreeNode*> model_nodes_;
    std::unordered_map<Index, TreeNode*> component_nodes_;
    int next_fake_id_ = -2;
    //! @brief 是否已完成首次构建。
    //! @note 不变式：initialized_ == true 蕴含 modelQuery_ != nullptr。
    //!       true 仅由 refresh() 在非空查询下置位；setModelQuery() 变更或置空时清除。
    bool initialized_ = false;
    QTimer* data_sync_timer_ = nullptr;
    std::unordered_set<Index> pending_model_ids_;
    std::unordered_set<Index> pending_component_ids_;
    std::unordered_map<int, bool> components_visibility_;
    std::unordered_map<int, std::pair<bool, bool>> sub_visibility_; // first=mesh visible, second=geometry visible
};
