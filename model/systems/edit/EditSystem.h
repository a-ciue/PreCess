/**
 * @file EditSystem.h
 * @author (your name)
 */
#pragma once
#include "Core.h"
#include "EditInfo.h"
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

namespace systems::edit {
class EditHandler;
struct HandlerMetaData {
    std::string name {}; // 模型编辑操作唯一名称，用作索引
    std::string display_name {}; // 模型编辑操作UI展示用名称
};

/**
 * @brief 模型编辑操作系统，负责管理和调用各种模型编辑操作处理器。所有模型编辑操作的入口。
 */
class EditSystem {
public:
    using SystemHandler = EditHandler; //> 模型编辑操作处理器类型，模型编辑操作系统下所有处理器的基类类型
    using SystemHandlerPtr = ::systems::SystemHandlerPtr<SystemHandler>; //> 处理器的智能指针，支持自定义析构函数。特别是兼容跨dll边界获取的析构函数。
    static const std::string name; //> 系统唯一名称，用于插件注册时的识别

    EditSystem(ModelManager& model_manager);
    ~EditSystem();
    /**
     * @brief 模型编辑操作调用接口
     * @param unique_name 模型编辑操作唯一名称
     * @param model
     * @param args 模型编辑操作参数
     */
    std::any call(const std::string& unique_name, Index component_id, const std::vector<core::ArgObject>& args);
    /**
     * @brief 注册模型编辑操作处理器插件
     */
    bool registerHandler(const HandlerMetaData& meta_data, SystemHandlerPtr handler);
    /**
     * @brief 注销模型编辑操作处理器插件
     */
    void unregisterHandler(const HandlerMetaData& meta_data);
    /**
     * @brief 获取已注册模型编辑操作类型信息
     */
    std::vector<EditInfo*> getEditInfos();
    /**
     * @brief 获取参数类型
     * @param unique_name 模型编辑操作唯一名称
     * @return 参数类型
     */
    std::optional<std::vector<core::ArgType>> getArgTypes(const std::string& unique_name);
    /**
     * @brief 设置算法信息变更回调函数
     */
    void setOnEditInfoChangedCallback(std::function<void()> callback);

private:
    ModelManager* model_manager_; //< 模型管理器引用，用于获取模型操作接口
    std::unordered_map<std::string, SystemHandlerPtr> handlers_; //< 模型编辑操作处理器插件列表，key为模型编辑操作唯一名称name
    std::unordered_map<std::string, std::unique_ptr<EditInfo>> edit_infos_; //< 模型编辑操作信息列表，key为模型编辑操作唯一名称name

    std::function<void()> on_edit_info_changed_; //< 编辑信息变更回调
};
}
