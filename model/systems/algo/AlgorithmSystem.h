/**
 * @file AlgorithmSystem.h
 * @author (your name)
 */
#pragma once
#include "AlgorithmInfo.h"
#include "Core.h"
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
class ModelManager;

namespace systems::io {
class ModelIOSystem;
}

namespace systems::algo {
class AlgorithmHandler;
struct HandlerMetaData {
    std::string name {}; // 算法唯一名称，用作索引
    std::string display_name {}; // 算法UI展示用名称
};

class AlgorithmSystem {
public:
    using SystemHandler = AlgorithmHandler; //> 算法处理器类型，算法系统下所有处理器的基类类型
    using SystemHandlerPtr = ::systems::SystemHandlerPtr<SystemHandler>; //> 处理器的智能指针，支持自定义析构函数。特别是兼容跨dll边界获取的析构函数。
    static const std::string name; //> 系统唯一名称，用于插件注册时的识别

    AlgorithmSystem(io::ModelIOSystem& io_system, ModelManager& model_manager);
    ~AlgorithmSystem();
    /**
     * @brief 算法调用接口
     * @param unique_name 算法唯一名称
     * @param component_id
     * @param args 算法参数
     */
    std::any call(const std::string& unique_name, Index component_id, const std::vector<core::ArgObject>& args);
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
    io::ModelIOSystem* io_system_; //< 模型IO系统引用，用于模型读写
    ModelManager* model_manager_; //< 模型管理器引用，用于获取模型操作接口
    std::unordered_map<std::string, SystemHandlerPtr> handlers_; //< 算法处理器插件列表，key为算法唯一名称name
    std::unordered_map<std::string, std::unique_ptr<AlgorithmInfo>> algorithm_infos_; //< 算法信息列表，key为算法唯一名称name

    std::function<void()> on_algorithm_infos_changed_;
};
}
