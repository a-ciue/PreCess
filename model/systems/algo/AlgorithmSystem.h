/**
 * @file AlgorithmSystem.h
 * @author (your name)
 */
#pragma once
#include "AlgorithmInfo.h"
#include "Core.h"

#include <any>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class ModelManager;

namespace systems {
template <typename Handler>
class PluginHandler;
}

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
    using Handler = AlgorithmHandler; //> 算法处理器类型
    using PluginHandler = ::systems::PluginHandler<Handler>; //> 插件处理器类型，跨dll动态库从插件中获取的，可据此访问插件中的处理器Handler对象。
    static const std::string name; //> 系统唯一名称，用于插件注册时的识别

    AlgorithmSystem(io::ModelIOSystem& io_system, ModelManager& model_manager);
    ~AlgorithmSystem();
    /**
     * @brief 算法调用接口
     * @param unique_name 算法唯一名称
     * @param model
     * @param args 算法参数
     */
    std::any call(const std::string& unique_name, Index model, const std::vector<std::any>& args);
    /**
     * @brief 注册算法处理器插件
     */
    bool registerHandler(const HandlerMetaData& meta_data, std::unique_ptr<PluginHandler> plugin_handler);
    /**
     * @brief 注销算法处理器插件
     */
    void unregisterHandler(const HandlerMetaData& meta_data);
    /**
     * @brief 获取已注册算法类型信息
     */
    std::vector<AlgorithmInfo*> getAlgorithmInfos();

private:
    io::ModelIOSystem* io_system_; //< 模型IO系统引用，用于模型读写
    ModelManager* model_manager_; //< 模型管理器引用，用于获取模型操作接口
    std::unordered_map<std::string, std::unique_ptr<PluginHandler>> plugin_handlers_; //< 算法处理器插件列表，key为算法唯一名称name
    std::unordered_map<std::string, std::unique_ptr<AlgorithmInfo>> algorithm_infos_; //< 算法信息列表，key为算法唯一名称name
};
}
