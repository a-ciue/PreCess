#pragma once

#include <QAbstractItemModel>
#include <QVariant>
#include <QVector>
#include <QtQml/qqmlregistration.h>
#include <span>
#include <unordered_map>

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
        NameRole = Qt::UserRole + 1,
        NumberRole,
        NodeIdRole,
        IsVisibleRole,
        ComponentIdRole,
        NodeTypeRole,
        VisibilityStateRole,
        CanToggleVisibilityRole
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

    //! @brief 统一对账入口；普通刷新不 reset，只有查询替换/清空才 reset。
    Q_INVOKABLE bool refresh();
    //! @brief 所有数据/结构通知的唯一入口，经 50ms 定时器合并后统一对账。
    Q_INVOKABLE void requestRefresh();
    //! @brief 只发送节点显隐意图；展示状态由管线回传后更新。
    Q_INVOKABLE bool requestVisibility(const QModelIndex& idx, bool visible);
    Q_INVOKABLE QModelIndex findIndexByNodeId(int nodeId, int depth) const;
    //! @brief 按模型发送全部节点的显隐请求，不提前修改展示副本。
    Q_INVOKABLE void requestAllVisibility(bool visible);

    /**
     * @brief 接收 GUI 管线回传的完整组件显隐结果（不是用户显隐命令）
     * @param component_id 组件 id；组件尚未入树时只保存，结构同步创建后应用
     * @param mesh_visible 网格总开关
     * @param geometry_visible 几何总开关
     * @note 这是显隐状态唯一写入点；对象树的用户入口只发请求。
     */
    Q_INVOKABLE void syncComponentVisibility(Index component_id, bool mesh_visible, bool geometry_visible);

    QObject* getModelQuery() const { return modelQuery_; }
    void setModelQuery(QObject* query);

signals:
    //! @brief 批量对账完成（数据/结构事件合并后）
    void dataSyncFinished();
    /** @brief 请求模型、组件、Mesh 或 Geometry 显隐；object_id 为真实模型或组件身份。 */
    void visibilityRequested(int node_type, int object_id, bool visible);

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
    //! @brief GUI 管线回传的组件子类型总开关。
    struct SubVisibility {
        bool mesh = true;
        bool geometry = true;
    };

    static int meshChildCount(const MeshNodeInfo& info, TreeNode::NodeType type);
    static int geometryChildCount(const GeometryNodeInfo& info, TreeNode::NodeType type);

    TreeNode* getNode(const QModelIndex& index) const;
    /** @brief 按组件保存的管线开关递归同步子节点展示状态，返回是否发生变化。 */
    bool syncSubNodes(TreeNode* node, bool notify);
    /** @brief 聚合节点的全显、部分显示和全隐状态。 */
    int visibilityState(const TreeNode* node) const;
    /** @brief 只通知祖先节点，不重复发送已单独处理的子树。 */
    void notifyAncestors(TreeNode* node);
    SubVisibility subVisibility(Index component_id) const;

    ModelNodeInfo queryModel(Index model_id) const;
    void fillComponentData(ComponentNodeInfo& info) const;

    TreeNode* buildModelNode(const ModelNodeInfo& info);
    TreeNode* buildComponentNode(const ComponentNodeInfo& info);
    TreeNode* buildMeshNode(const MeshNodeInfo& info);
    TreeNode* buildGeometryNode(const GeometryNodeInfo& info);

    void syncModelNode(TreeNode* node, const ModelNodeInfo& info);
    void syncComponentNode(TreeNode* node, const ComponentNodeInfo& info);
    void syncMeshNode(TreeNode* node, const MeshNodeInfo& info);
    void syncGeometryNode(TreeNode* node, const GeometryNodeInfo& info);

    struct CountChildSpec {
        TreeNode::NodeType type;
        const char* name;
        int count = 0;
    };
    void syncCountChildren(TreeNode* node, std::span<const CountChildSpec> specs);

    void insertChildNode(TreeNode* parent, int row, TreeNode* node);
    void removeChildNode(TreeNode* node);
    void moveChildNode(TreeNode* parent, int from, int to);
    QModelIndex indexOfNode(TreeNode* node) const;
    void notifyNodeData(TreeNode* node, const QVector<int>& roles);

    void pruneStaleVisibility();
    void reconcileAllFromQuery();
    //! @brief 重建渲染回传使用的组件查找缓存；结构对账不依赖该索引。
    void rebuildComponentLookup();
    void flushPendingRefresh();

    TreeNode* rootNode = nullptr;
    QModelQuery* modelQuery_ = nullptr;
    int next_fake_id_ = -2;
    QTimer* refresh_timer_ = nullptr;
    bool refresh_pending_ = false;
    bool refreshing_ = false;
    //! @brief GUI 管线回传的子类型总开关；缺省为可见，对象树不写用户设置。
    std::unordered_map<Index, SubVisibility> sub_visibility_;
    //! @brief 渲染回传的组件节点查找缓存；仅用于显隐同步，不参与结构权威。
    std::unordered_map<Index, TreeNode*> component_lookup_;
};
