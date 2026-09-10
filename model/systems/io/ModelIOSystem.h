/**
 * @file ModelIOSystem.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#pragma once
#include "Core.h"
#include "ModelIOSystemBase.h"
#include "SystemHandlerPtr.h"
#include <any>
#include <filesystem>
#include <functional>
#include <unordered_map>
#include <vector>

class ModelLayer;

namespace systems::io {
class ModelIOHandler;
struct ModelIOInfo;
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

    ModelIOSystem(ModelLayer& manager);
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
    /**
     * @brief 推导导出建议文件名：以模型名为主干，换成目标文件类型的首选扩展名
     *
     * 模型名多来自导入文件名（自带扩展名）：目标类型已注册时先剥掉原有扩展名再换上目标扩展名，
     * 避免叠出 "a.obj.stl" 这类名字；目标类型未注册（如界面默认的 "All files"）时原样返回模型名，
     * 此时模型名自带的扩展名是唯一能反查文件类型的线索，不能被抹掉。
     * @param model_name 模型名（文件名，不含目录），允许为空
     * @param file_type 目标文件类型，应在注册的文件类型中，允许未注册
     * @return 建议文件名（含扩展名），模型名为空或目标类型无首选扩展名时返回入参模型名
     */
    std::string suggestFileName(const std::string& model_name, const std::string& file_type) const;
    /**
     * @brief 写出前按目标文件类型校正路径扩展名
     *
     * 文件对话框的预填名可能不随用户改选的文件类型变化，这里按最终选中的类型校正：
     * 扩展名缺失或属于其他已注册类型时换成目标类型的首选扩展名，
     * 扩展名已属于目标类型、或属于未注册的自定义扩展名（用户显式指定）时保持不动。
     * @param path 待写出的文件路径，本地系统环境编码
     * @param file_type 目标文件类型，应在注册的文件类型中，允许未注册
     * @return 校正后的路径，无需校正或目标类型无首选扩展名时原样返回
     */
    std::filesystem::path adaptFileExtension(const std::filesystem::path& path, const std::string& file_type) const;

private:
    /**
     * @brief 取文件类型的首选（第一个）扩展名
     * @param file_type 文件类型，允许未注册
     * @return 扩展名（不含点），文件类型未注册或未声明扩展名时返回空串
     */
    std::string preferredExtension(const std::string& file_type) const;
    /**
     * @brief 判断扩展名是否被某个已注册文件类型支持
     * @param extension 扩展名（不含点），大小写不敏感
     */
    bool isRegisteredExtension(const std::string& extension) const;

    ModelLayer* manager_;
    std::unordered_map<std::string, SystemHandlerPtr> handlers_; //> 键是文件类型
    std::unordered_map<std::string, std::unique_ptr<ModelIOInfo>> file_type_infos_; //> 键是文件类型，值是支持的文件类型信息(如扩展名、参数信息、描述等)

    std::function<void()> on_dialog_name_filters_changed_;
};
}
