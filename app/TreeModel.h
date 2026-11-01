#pragma once

#include <QAbstractItemModel>
#include <QVariant>
#include <QVector>
#include <QtQml/qqmlregistration.h>
#include <optional>
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
     * @brief 接收渲染层的完整组件显隐结果（命令回传，不是用户显隐命令）
     * @param component_id 组件 id；组件已不在模型时忽略
     * @param mesh_visible 网格显隐结果
     * @param geometry_visible 几何显隐结果
     * @note 不以树节点是否存在为接收条件：节点尚未入树时只保存，结构同步创建后应用；
     *       节点存在时一次应用完整结果并通知，不再复用带副作用的用户显隐命令。
     */
    Q_INVOKABLE void applyComponentVisibility(Index component_id, bool mesh_visible, bool geometry_visible);

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
    //! @brief 增量同步：组件移除（结构事件，立即执行）
    Q_INVOKABLE void syncComponentRemoved(Index component_id);

    //! @brief 数据事件合并入口：50ms 内同一模型只同步一次
    Q_INVOKABLE void queueModelChanged(Index model_id);
    //! @brief 组件变化入口：映射到所属模型后按 50ms 合并
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

    ModelNodeInfo queryModel(Index model_id) const;
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
    void eraseComponentVisibility(Index component_id);
    void pruneStaleVisibility();
    //! @brief 组件级显隐开关，缺省为可见。
    bool isComponentEnabled(Index component_id) const;
    //! @brief 按已保存的设置应用一个组件的显隐：设置是输入，节点 isVisible 只作派生结果。
    void applyComponentVisibilityState(TreeNode* component_node, bool notify);
    void setSubtreeVisibility(TreeNode* node, bool visible, bool notify);
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
    //! @brief 组件级显隐设置（空组件据此显示；非空组件作为子节点总开关）。
    std::unordered_map<Index, bool> components_visibility_;
    //! @brief 子类型显隐设置；空值表示尚未收到设置，子类型出现时默认可见。
    //! @note 与节点 isVisible 分离：结构同步不得用派生结果覆盖这里的设置。
    struct SubVisibility {
        std::optional<bool> mesh;
        std::optional<bool> geometry;
    };
    std::unordered_map<Index, SubVisibility> sub_visibility_;
};
