/**
 * @file ModelIOSystem.h
 * @author 张家僮(htxz_6a6@163.com)
 */
//#pragma once
#ifndef MODEL_IO_HANDLER_H
#define MODEL_IO_HANDLER_H
#include <memory>
#include <vector>
#include <string>
#include <filesystem>
#include <any>

class ModelData;
class ArgType;

namespace systems::io {
namespace fs = std::filesystem;
using std::vector;
using std::unique_ptr;
using std::string;

/**
 * @brief 模型IO处理器接口
 */
class ModelIOHandler {
public:
	virtual ~ModelIOHandler() = default; 
	/**
	 * @brief 读取模型功能
	 * @param path 待读取文件路径
	 * @param args 读取文件要传入参数
     * @return 构造的模型数据对象
	 */
	virtual unique_ptr<ModelData> read_model(const fs::path& path, const vector<std::any>& args) = 0;
	/**
	 * @brief 写出模型功能
	 * @param data 待写出模型数据对象
	 * @param path 写出文件目标路径
	 * @param args 写出文件要传入参数
	 */
	virtual void write_model(const ModelData& data, const fs::path& path, const vector<std::any>& args) = 0;

	/**
	 * @brief 读取文件参数类型，交给UI使用
	 * @return 返回参数类型列表
	 */
	virtual vector<ArgType> read_args_type() const = 0;
	/**
	 * @brief 写出文件参数类型，交给UI使用
	 * @return 返回参数类型列表
	 */
	virtual vector<ArgType> write_args_type() const = 0;
	/**
	 * @brief 处理的文件格式，每种拓展名都对应一个文件格式
	 * @return 文件类型名称，取Wikipedia上对应模型类型词条名称，如"Wavefront .obj file", "ISO 10303-21", "STL (file format)"等
	 */
	virtual string file_type() const = 0;
	/**
	 * @brief 该处理器支持的文件扩展名列表
	 * @return 支持的文件扩展名列表，因为一种文件格式可能有多个扩展名，如["step", "stp"]
	 */
	virtual vector<string> file_extensions() const = 0;
};
}
#endif // MODEL_IO_HANDLER_H
