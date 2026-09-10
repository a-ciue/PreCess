/**
 * @file Session.h
 * @brief 会话组合根：模型层、undo 栈、事件总线与四个系统的装配与有序拆解（不依赖 Qt）
 *
 * Session 是原 app 层 QModelManager 装配逻辑的无 Qt 下沉：构造时按依赖序组装
 * ModelLayer / UndoStack / EventBus / ModelIO / Algo / Edit / Feature 系统，
 * 并经内部转发观察者把模型层通知桥接为 ModelEvent（功能系统的 ModelEvent
 * 订阅在无 Qt 宿主下照常工作）。Qt 侧适配器（QML 信号、参数回写桥）仍由
 * app 层 QModelManager 挂接。析构（或 teardown）按依赖逆序有序拆解。
 */
#ifndef SESSION_H
#define SESSION_H
#include "ComponentOperator.h" // boundaryWrite 参数需完整类型
#include "EventBus.h" // 成员为值类型，需完整定义
#include "ModelLayer.h" // 成员为值类型，需完整定义
#include "SessionQuery.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace systems {
class SystemPluginManager;
}
namespace systems::io {
class ModelIOSystem;
}
namespace systems::algo {
class AlgorithmSystem;
}
namespace systems::edit {
class EditSystem;
}
namespace systems::feature {
class FeatureSystem;
}
class UndoStack;

namespace session {

class Session {
public:
    /**
     * @brief 装配会话
     * @param observer 宿主观察者（如 app 层 QModelObserver；可为 nullptr）。
     *                 内部转发观察者先通知宿主、再把模型层通知桥接为 ModelEvent。
     */
    explicit Session(ModelObserver* observer = nullptr);
    //! @brief 有序拆解（见 teardown），保证功能停用与钩子断开先于成员析构
    ~Session();

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    // —— 子系统访问 ——
    ModelLayer& model() { return model_; }
    SessionQuery& query() { return query_; }
    UndoStack& undoStack() { return *undo_stack_; }
    core::EventBus& events() { return event_bus_; }
    systems::io::ModelIOSystem& ioSystem() { return *io_system_; }
    systems::algo::AlgorithmSystem& algorithmSystem() { return *algo_system_; }
    systems::edit::EditSystem& editSystem() { return *edit_system_; }
    systems::feature::FeatureSystem& featureSystem() { return *feature_system_; }
    systems::SystemPluginManager& pluginManager() { return *plugin_manager_; }

    // —— 插件装载（Qt 插件机制，供宿主进程调用）——
    //! @brief 注册全部静态插件
    void loadStaticPlugins();
    //! @brief 扫描目录逐个注册动态插件；目录不存在时记日志跳过
    void loadPluginsFromDirectory(const std::filesystem::path& plugin_dir);

    // —— 模型结构操作入口（QML 适配层与脚本宿主共用）——
    void removeModel(Index model_id);
    void removeComponent(Index component_id);
    //! @brief 移除组件网格（操作边界：undo 自动记录 + 通知统一 flush）
    void removeMesh(Index component_id);
    //! @brief 移除组件几何（操作边界：undo 自动记录 + 通知统一 flush）
    void removeGeometry(Index component_id);

    //! @brief 有序拆解（幂等）：先停功能系统，再断开栈与模型层的互指钩子
    void teardown();

private:
    /**
     * @brief 内部转发观察者：模型层通知先转发宿主观察者，再桥接为 ModelEvent
     */
    class ObserverRelay;

    //! @brief 组件写操作的操作边界包装（undo 自动记录 + 异常先提交再 flush 重抛）
    void boundaryWrite(Index component_id, std::string label, const std::function<void(ComponentOperator&)>& write);

    // 声明序即依赖序（析构为逆序）：event_bus_ 最先构造、最后析构，
    // relay 转发桥与功能系统在拆解期始终可用
    core::EventBus event_bus_; //> 事件总线（ModelEvent 等的发布中枢）
    std::unique_ptr<ObserverRelay> relay_; //> 模型层通知转发 + ModelEvent 桥接
    ModelLayer model_;
    SessionQuery query_;
    std::unique_ptr<UndoStack> undo_stack_;
    std::unique_ptr<systems::io::ModelIOSystem> io_system_;
    std::unique_ptr<systems::algo::AlgorithmSystem> algo_system_;
    std::unique_ptr<systems::edit::EditSystem> edit_system_;
    std::unique_ptr<systems::feature::FeatureSystem> feature_system_;
    std::unique_ptr<systems::SystemPluginManager> plugin_manager_;
    bool torn_down_ { false }; //> teardown 幂等守卫
};
}
#endif // SESSION_H
