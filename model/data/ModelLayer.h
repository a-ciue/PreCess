/**
 * @file ModelLayer.h
 * @brief 负责管理多个模型实例的类
 *
 * ModelLayer 仅负责管理模型（添加、删除、查询、重命名）以及模型事件的发送。
 *
 * @author 徐昊阳 haoyangxu06@gmail.com
 * @date 2025/3/20
 */
#ifndef MODEL_MANAGER_H
#define MODEL_MANAGER_H
#include "ModelData.h"

#include "ComponentOperator.h" // MeshEditKind（markComponentDirty 默认参数）
#include "GeometryRegistry.h"
#include "MeshIDMap.h"
#include "ModelOperator.h"

#include <TopAbs_ShapeEnum.hxx>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <source_location>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

class ModelObserver; // 前向声明模型观察者类
namespace session {
class SessionQuery; // 会话层查询（友元，访问 models_ / component_to_model_ 私有表）
}
class UndoRecorder; // 前向声明 undo 记录钩子接口
struct ModelSnapshot; // 前向声明模型级结构快照

//! @brief 框架忙碌拒绝：桥接层统一处理，插件无需自行捕获。
class ModelOperationBusy : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief 负责管理多个 ModelData 实例的类
 *
 * ModelLayer 允许动态添加、删除和查找模型，通过Observer模式发送模型事件。
 * 使得 QML 层能够访问和控制网格数据。
 */
class ModelLayer {
    struct WriteAuthority;

public:
    /**
     * @brief 构造 ModelLayer 对象
     *
     * @param parent 父对象，默认为 nullptr
     * @param observer 模型观察者对象，用于捕获模型事件（默认 nullptr）
     */
    explicit ModelLayer(ModelObserver* observer = nullptr)
        : observer_(observer)
    {
    }

    /**
     * @brief 按值移动（测试夹具/返回值场景）：数据搬移，写权限状态与来源**共享**
     *
     * 来源移动后仍可安全析构与查询（不因权限指针悬空而崩溃）；移动只搬数据、
     * 不分裂权限——影子/真实层的闸判定与构造时一致。
     */
    ModelLayer(ModelLayer&& other) noexcept;

    /**
     * @brief 添加一个模型
     *
     * @param model_name 新模型的名称（UTF-8 编码）
     * @param model 需要添加的模型对象
     */
    Index addModel(const std::string& model_name, ComponentDatas components);

    //! @brief 取整模型深拷贝快照（撤销 removeModel / 重做 addModel 用）
    std::unique_ptr<ModelSnapshot> takeModelSnapshot(Index model_id) const;

    /**
     * @brief 按快照原 id 恢复模型（组件入池、component_to_model_、gid reclaim、几何索引重建）
     * @return 恢复出的模型 id（即快照原 id）
     * @throw std::runtime_error 原 model_id/component_id 已被占用，或 gid reclaim 冲突
     */
    Index restoreModel(const ModelSnapshot& snapshot);

    /**
     * @brief 按组件自带 id 把组件插回指定模型（撤销 removeComponent / 重做 addGeometryComponent 用）
     * @throw std::runtime_error model 不存在或 component id 已被占用，或 gid reclaim 冲突
     */
    void restoreComponent(Index model_id, std::unique_ptr<ComponentData> component);

    /**
     * @brief 移除指定名称的模型
     *
     * @param model_id 需要移除的模型 ID
     */
    void removeModel(Index model_id);
    void removeComponent(Index component_id);

    /**
     * @brief 获取指定模型的操作接口对象
     *
     * 如果对应模型的 ModelOperator 不存在，则创建并返回新的 ModelOperator。
     * ModelOperator 封装模型数据的操作接口，用于执行命令等操作。
     *
     * @param model_id 模型 ID
     * @return 对应模型名称的 ModelOperator 对象指针
     */
    std::optional<ModelOperator> getModelOperator(Index model_id);
    ModelData* modelById(Index model_id);
    const ModelData* modelById(Index model_id) const;
    std::optional<ComponentOperator> getComponentOperator(Index component_id);

    ComponentData* findComponent(Index component_id);
    const ComponentData* findComponent(Index component_id) const;

    /**
     * @brief 根据几何形状类型和全局 ID 查找所属 Component。
     * @param shape_type 几何形状类型，支持 Vertex、Edge、Face 和 Solid。
     * @param shape_id 几何形状的全局 ID。
     * @return 所属 Component ID；类型不支持或未找到时返回空。
     */
    std::optional<Index> findComponentIdByGeometryShapeId(
        TopAbs_ShapeEnum shape_type,
        Index shape_id) const;

    GeometryRegistry& geomRegistry();
    const GeometryRegistry& geomRegistry() const;

    MeshIDMap& pointIdMap();
    const MeshIDMap& pointIdMap() const;

    MeshIDMap& edgeIdMap();
    const MeshIDMap& edgeIdMap() const;

    /**
     * @brief 标记组件数据已修改（写路径自动调用；Topology 类立即失效邻接懒表并记入待通知集合）
     * @note 通知不即时发出，由操作边界 flushNotifications() 统一发 notifyComponentChanged
     * @param loc 写入口调用点（无归属写诊断用，由 ComponentOperator 写入口默认捕获）
     */
    void markComponentDirty(Index component_id, MeshEditKind kind = MeshEditKind::Topology,
        std::source_location loc = std::source_location::current());

    //! @brief 对待通知集合逐组件发 notifyComponentChanged 并清空（操作边界调用；空集合无操作）
    void flushNotifications();

    //! @brief 挂接 undo 记录钩子（默认 nullptr，测试/无栈场景零开销）
    void setUndoRecorder(UndoRecorder* recorder) { undo_recorder_ = recorder; }

    //! @brief 按引用反查模型 id（undo 记录钩子等场景）；未找到返回 -1
    Index findModelId(const ModelData& model) const;

    /**
     * @brief 模型操作占用：身份绑定，只有本操作能够释放；取消不释放。
     * @note 申请、查询、释放与析构均属于模型所属线程；worker 不持有此凭证。
     */
    class WriteOperation {
    public:
        WriteOperation(std::shared_ptr<WriteAuthority> authority, std::uint64_t id);
        ~WriteOperation();
        void release() noexcept;
        bool active() const noexcept;
        WriteOperation(const WriteOperation&) = delete;
        WriteOperation& operator=(const WriteOperation&) = delete;

    private:
        friend class ModelLayer;
        friend class WritePrivilege;
        std::shared_ptr<WriteAuthority> authority_;
        std::uint64_t id_;
    };

    /**
     * @brief 在模型所属线程申请独占操作凭证；已有操作时返回空。
     * @param masked 是否驱动交互遮罩（只影响展示，所有操作均拒绝旁路模型写）
     */
    std::unique_ptr<WriteOperation> beginWriteOperation(bool masked = false);

    /**
     * @brief 当前操作的 GUI 写段授权；线程检查始终先于授权检查。
     * @note 未提供操作凭证时不能跨越操作占用，只能嵌套已授权的框架写段。
     */
    class WritePrivilege {
    public:
        explicit WritePrivilege(ModelLayer& layer, const WriteOperation* operation = nullptr);
        ~WritePrivilege();
        WritePrivilege(const WritePrivilege&) = delete;
        WritePrivilege& operator=(const WritePrivilege&) = delete;

    private:
        ModelLayer* layer_;
    };

    //! @brief 影子层声明：允许 worker 写自身副本，真实模型始终保持线程亲和。
    void setOffthreadWritesAllowed(bool on) noexcept { write_authority_->offthread_ok = on; }
    //! @brief 展示遮罩状态，由当前操作类别派生。
    bool writesFrozen() const noexcept { return writesPending() && write_authority_->masked; }
    //! @brief 模型操作是否仍在占用（准备、计算、提交与收尾共用一个状态源）。
    bool writesPending() const noexcept { return write_authority_->operation != 0; }
    //! @brief 当前 GUI 段是否持有操作写授权（undo 恢复与框架提交查询）。
    bool hasWritePrivilege() const noexcept;
    //! @brief 宿主操作及模型事件只能在模型所属线程执行。
    void assertOwnerThread() const;
    //! @brief 注册或宿主装配须在所属线程且模型空闲；写段授权不豁免占用。
    void assertOperationIdle(std::source_location loc = std::source_location::current()) const;

private:
    Index allocateComponentId() noexcept;

    //! @brief 统一写前检查：线程、操作授权及记账归属；拒绝时数据与缓存均未改变。
    void assertWriteAllowed(std::source_location loc = std::source_location::current()) const;

    /**
     * @brief 按给定 id 把组件纳入指定模型（入全局池、登记 component_to_model_、gid 对账）
     *
     * gid 对账顺序：几何索引 ensureIndexBuilt（快照克隆体索引未建，此处重建领新 gid）→
     * 点 gid 先 reclaimPointGlobalIds 按原值回收再 ensurePointGlobalIds 补缺 →
     * 边 gid 先 reclaimEdgeGlobalIds 再 ensureEdgeGlobalIds 补缺；reclaim 与 ensure 幂等兼容，
     * addModel（gid 尚未分配，reclaim 自然无操作）与快照恢复共用本路径。
     * @throw std::runtime_error model 不存在或 component id 已被占用，或 gid reclaim 冲突
     */
    void adoptComponent(Index component_id, std::unique_ptr<ComponentData> component, Index model_id);

    GeometryRegistry geom_registry_;

    std::unordered_map<Index, std::unique_ptr<ModelData>> models_;
    std::unordered_map<Index, std::unique_ptr<ComponentData>> components_; // 全局组件池
    std::unordered_map<Index, Index> component_to_model_;
    Index max_index_ { -1 }; //!< 最大索引值，用于唯一标识模型
    Index next_component_id_ { 0 }; //!< component_id 全局发号器（只增不减）

    MeshIDMap point_id_map_; // 点的 global<->local（gid 为纯身份标识，坐标由组件 MeshData 自持）
    MeshIDMap edge_id_map_; // 边的 global->local

    ModelObserver* observer_ { nullptr }; //!< 全局模型观察者，用于捕获模型事件
    std::vector<Index> pending_notify_; //!< 本次操作内被标脏的组件（去重；undo 操作记录的预留拦截点）
    UndoRecorder* undo_recorder_ { nullptr }; //!< undo 记录钩子（写前/结构操作时机回调，由 UndoStack 实现）
    bool component_remove_hook_suspended_ { false }; //!< removeModel 期间抑制组件级移除钩子（组件随模型快照整体记录，避免重复）

    /**
     * @brief 写权限状态（堆上共享）：含线程亲和、冻结窗口与框架特权深度
     *
     * 所有占用／授权变更均在所属线程，worker 只访问独立影子，不需要原子释放协议。
     * 移动模型时共享控制块，既有操作凭证仍能按原身份释放。
     */
    struct WriteAuthority {
        std::thread::id owner_thread { std::this_thread::get_id() }; //!< undo 记账线程（构造线程，不变）
        bool offthread_ok { false }; //!< 影子层豁免线程闸（构造后一次性置位）
        std::uint64_t operation { 0 }; //!< 当前操作身份，0 表示空闲
        bool masked { false }; //!< 当前操作的展示类别
        std::uint64_t next_operation { 0 }; //!< 仅由模型所属线程发号
        int privilege { 0 }; //!< 框架写特权深度（仅持特权线程自身增减）
    };
    std::shared_ptr<WriteAuthority> write_authority_ { std::make_shared<WriteAuthority>() };

    friend class session::SessionQuery;
    friend class ModelOperator;
};
#endif // MODEL_MANAGER_H
