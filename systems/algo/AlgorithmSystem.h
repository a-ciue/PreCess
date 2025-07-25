/**
 * @file AlgorithmSystem.h
 * @author (your name)
 */
#pragma once
#include "AlgorithmHandler.h"
#include "AlgorithmInfo.h"
#include <any>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace systems::io {
class ModelIOSystem;
}

namespace systems::algo {
using std::string;
using std::unordered_map;
using std::vector;

struct HandlerMetaData {
    string name {}; // 算法唯一名称，用作索引
    string display_name {}; // 算法UI展示用名称
};

class AlgorithmSystem {
public:
    using Handler = AlgorithmHandler;
    static const string name;

    AlgorithmSystem(io::ModelIOSystem& io_system, ModelManager& model_manager);
    /**
     * @brief 算法调用接口
     * @param unique_name 算法唯一名称
     * @param model
     * @param args 算法参数
     */
    std::any call(const string& unique_name, Index model, const vector<std::any>& args);
    /**
     * @brief 注册算法处理器
     */
    bool registerHandler(const HandlerMetaData& meta_data, std::shared_ptr<AlgorithmHandler> handler);
    /**
     * @brief 注销算法处理器
     */
    void unregisterHandler(const HandlerMetaData& meta_data);
    /**
     * @brief 获取已注册算法类型
     */
    vector<AlgorithmInfo*> getAlgorithmInfos();

private:
    io::ModelIOSystem* io_system_; //< 模型IO系统引用，用于模型读写
    ModelManager* model_manager_; //< 模型管理器引用，用于获取模型操作接口
    unordered_map<string, std::shared_ptr<AlgorithmHandler>> handlers_; //< 算法处理器列表，key为算法唯一名称name
    unordered_map<string, std::unique_ptr<AlgorithmInfo>> algorithm_infos_; //< 算法信息列表，key为算法唯一名称name
};
}
