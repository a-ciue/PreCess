/**
 * @file AlgorithmSystem.h
 * @author (your name)
 */
#pragma once
#include "AlgorithmInfo.h"
#include "Core.h"
#include "Job.h"
#include "SystemHandlerPtr.h"

#include <any>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace core {
class ArgObject;
}
class ModelLayer;
class ComponentOperator;

namespace systems::io {
class ModelIOSystem;
}

class UndoStack;

namespace systems::job {
class JobRunner;
struct JobWork;
}

namespace systems::algo {
class AlgorithmHandler;
//! 进度回调别名：任务系统（systems::job）定义公共类型，算法侧沿用短名
using ProgressFn = systems::job::ProgressFn;
/**
 * @brief 影子执行预备态（定义于 AlgorithmSystem.cpp，对外不透明）
 *
 * GUI 线程经 prepareAlgorithm 构建（解析目标 + 快照 + 影子层），
 * 工作线程消费 computeShadow，GUI 线程内部应用影子。
 */
struct PreparedAlgorithm;
struct HandlerMetaData {
    std::string name { }; // 算法唯一名称，用作索引
    std::string display_name { }; // 算法UI展示用名称
};

class AlgorithmSystem {
public:
    using SystemHandler = AlgorithmHandler; //> 算法处理器类型，算法系统下所有处理器的基类类型
    using SystemHandlerPtr = ::systems::SystemHandlerPtr<SystemHandler>; //> 处理器的智能指针，支持自定义析构函数。特别是兼容跨dll边界获取的析构函数。
    static const std::string name; //> 系统唯一名称，用于插件注册时的识别

    AlgorithmSystem(io::ModelIOSystem& io_system, ModelLayer& model_manager, UndoStack* undo_stack = nullptr);
    ~AlgorithmSystem();
    /**
     * @brief 算法调用接口
     * @param unique_name 算法唯一名称
     * @param component_id
     * @param args 算法参数
     * @param progress 可选进度回调（0~1 + 阶段名）；空则退化为 no-op，handler 内上报不生效
     */
    std::any call(const std::string& unique_name, Index component_id, const std::vector<core::ArgObject>& args,
        ProgressFn progress = nullptr);

    /** @brief 完整异步入口：所属线程准备，worker 计算，所属线程提交；不可影子化时同步执行。 */
    std::shared_ptr<systems::job::Job> callAsync(std::string unique_name, Index component_id,
        std::vector<core::ArgObject> args);
    //! @brief 组合根一次注入共享执行器，缺省为原生同步配置。
    void setJobRunner(systems::job::JobRunner* runner);
    /**
     * @brief 注册算法处理器插件
     */
    bool registerHandler(const HandlerMetaData& meta_data, SystemHandlerPtr handler);
    /**
     * @brief 注销算法处理器插件
     */
    void unregisterHandler(const HandlerMetaData& meta_data);
    /**
     * @brief 获取已注册算法类型信息
     */
    std::vector<AlgorithmInfo*> getAlgorithmInfos();
    /** @brief 从基础分类与当前算法声明派生分类快照；不维护独立的分类注册状态。 */
    std::vector<AlgorithmCategory> getNavigationCategories() const;
    /**
     * @brief 获取参数类型
     * @param unique_name 算法唯一名称
     * @return 参数类型
     */
    std::optional<std::vector<core::ArgType>> getArgTypes(const std::string& unique_name);
    /**
     * @brief 设置算法信息变更回调函数
     */
    void setOnAlgorithmInfosChanged(std::function<void()> callback);

private:
    //! @brief 同一注册的 handler 与信息共同持有，节点扩容不改变信息地址。
    struct AlgorithmEntry {
        SystemHandlerPtr handler;
        AlgorithmInfo info;
    };

    //! @brief 已解析真实目标的共同执行段；占用与预览处置由入口负责。
    std::any executeAlgorithm(const AlgorithmEntry& entry, ComponentOperator& target,
        const std::vector<core::ArgObject>& args, ProgressFn progress, const std::string& owner = { });
    systems::job::JobWork prepareAlgorithm(const std::string& unique_name, Index component_id,
        std::vector<core::ArgObject> args, const std::string& owner);
    void computeShadow(PreparedAlgorithm& prep, ProgressFn progress);
    void applyShadow(PreparedAlgorithm& prep);
    systems::job::JobRunner* job_runner_ { nullptr };
    io::ModelIOSystem* io_system_; //< 模型IO系统引用，用于模型读写
    ModelLayer* model_manager_; //< 模型管理器引用，用于获取模型操作接口
    UndoStack* undo_stack_ { nullptr }; //< undo 栈引用（可空：无栈时操作边界退化为仅 flush）
    std::unordered_map<std::string, AlgorithmEntry> entries_; //< 算法注册条目，key 为算法唯一名称

    std::function<void()> on_algorithm_infos_changed_;
};
}
