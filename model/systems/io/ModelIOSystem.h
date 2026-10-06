/**
 * @file ModelIOSystem.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#pragma once
#include "Core.h"
#include "ModelIOInfo.h"
#include "ModelIOSystemBase.h"
#include "ModelPayload.h"
#include "SystemHandlerPtr.h"
#include <any>
#include <filesystem>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

class ModelLayer;
class UndoStack;

namespace systems::io {
class ModelIOHandler;
/**
 * @brief 对应Handler的元信息
 */
struct HandlerMetaData {
    std::string file_type; //> 处理的文件格式，取Wikipedia上对应模型类型词条名称，如"Wavefront .obj file", "ISO 10303-21"；名称不得含圆括号（Qt 会按第一个'('截断文件对话框过滤器名），如"OFF (file format)"须写作"OFF file format"
    std::vector<std::string> extensions; //> 支持的文件扩展名列表，如["txt", "obj"]
};

/**
 * @brief 模型IO系统，每种文件格式只能注册一个处理器
 */
class ModelIOSystem : public ModelIOSystemBase {
public:
    using SystemHandler = ModelIOHandler;
    using SystemHandlerPtr = ::systems::SystemHandlerPtr<SystemHandler>;
    static const std::string name; //> 系统名称

    ModelIOSystem(ModelLayer& manager, UndoStack* stack = nullptr);
    ~ModelIOSystem() override;
    /**
     * @brief 系统的读模型接口
     * @param path 读取路径，本地系统环境编码
     * @param file_type 文件类型，应在注册的文件类型中
     * @param args 读操作的参数，传给Handler
     * @return 读取并添加模型成功返回true，失败（文件类型未注册或无法解析出模型）返回false
     */
    bool read(const std::filesystem::path& path, const std::string& file_type, const std::vector<std::any>& args) override;
    /**
     * @brief 仅解析文件为模型载荷，不落层
     * @return 解析成功返回载荷；文件类型未注册或解析失败返回空
     * @note 供按目标层落层的调用方（如算法影子执行）复用已注册格式 handler；
     *       纯文件解析，不触模型、无落层副作用
     */
    std::optional<ModelPayload> parseModel(const std::filesystem::path& path, const std::string& file_type,
        const std::vector<std::any>& args);
    /**
     * @brief 系统的写模型接口
     * @param model 模型id
     * @param path 写出路径，本地系统环境编码
     * @param file_type 文件类型，应在注册的文件类型中
     * @param args 写操作的参数，传给Handler
     */
    void write(Index model, const std::filesystem::path& path, const std::string& file_type, const std::vector<std::any>& args) override;
    void writeComponents(const std::vector<Index>& component_ids,
        const std::filesystem::path& path,
        const std::string& file_type,
        const std::vector<std::any>& args) override;
    /**
     * @brief 系统的功能Handler注册函数
     * @param meta_data 功能的元信息
     * @param handler 从插件读取的待注册的处理功能
     */
    bool registerHandler(const HandlerMetaData& meta_data, SystemHandlerPtr handler);
    /**
     * @brief 系统功能注销
     */
    void unregisterHandler(const HandlerMetaData& meta_data);
    /**
     * @brief 注册的文件类型
     * @return 键是文件类型，值是支持的文件类型信息(如扩展名、参数信息、描述等)
     */
    std::vector<ModelIOInfo*> registeredFileTypeInfos();
    /**
     * @brief 设置算法信息变更回调函数
     */
    void setOnDialogNameFiltersChanged(std::function<void()> callback);

private:
    //! @brief 文件格式处理器与信息共同持有，占用期注册表保持只读。
    struct IOEntry {
        SystemHandlerPtr handler;
        ModelIOInfo info;
    };

    ModelLayer* manager_;
    UndoStack* undo_stack_;
    std::unordered_map<std::string, IOEntry> entries_; //> 键是文件类型

    std::function<void()> on_dialog_name_filters_changed_;
};
}
