/**
 * @file ModelIOSystem.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef MODEL_IO_HANDLER_H
#define MODEL_IO_HANDLER_H

#include "Core.h"
#include "ModelPayload.h"

#include <memory>
#include <vector>
#include <string>
#include <filesystem>
#include <any>
#include <optional>

class ModelData;
struct ComponentData;
class ModelLayer;

namespace core {
struct ArgType;
}
namespace systems::io {
namespace fs = std::filesystem;
/**
 * @brief 模型IO系统的功能接口，继承他来实现具体的模型读写功能
 */
class ModelIOHandler {
public:
    virtual ~ModelIOHandler() = default; 
	/**
     * @brief 读取模型功能
     * @param path 待读取文件路径，本地系统环境编码
     * @param args 读取文件要传入参数
     * @return 构造的模型数据对象
     */
    virtual std::optional<ModelPayload> read_model(const fs::path& path, const std::vector<std::any>& args) = 0;
        /**
         * @brief 写出组件集合到文件
         * @param mgr ModelLayer（运行期权威：component pool + globalPoints）
         * @param component_ids 要写出的组件 id 集合
         * @param path 写出文件目标路径
         * @param args 写出文件要传入参数
         */
    virtual void write_components(const ModelLayer& mgr,
        const std::vector<Index>& component_ids,
        const fs::path& path,
        const std::vector<std::any>& args)
        = 0;

	/**
	 * @brief 读取文件参数类型，交给UI使用
	 * @return 返回参数类型列表
	 * TODO: 待存放到元数据
	 */
    virtual std::vector<core::ArgType> read_args_type() const = 0;
	/**
	 * @brief 写出文件参数类型，交给UI使用
	 * @return 返回参数类型列表
	 * TODO: 待存放到元数据
	 */
    virtual std::vector<core::ArgType> write_args_type() const = 0;
};
}
#endif // MODEL_IO_HANDLER_H
