/**
 * @file ModelIOSystem.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#pragma once
#include <any>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include "../../Core.h"

class ModelManager;

namespace systems::io {
using namespace std;
class ModelIOHandler;

/**
 * @brief 模型IO系统
 */
class ModelIOSystem {
public:
    ModelIOSystem(ModelManager& manager);
	/**
	 * @brief 系统的读模型接口
	 * @param path 读取路径
     * @param file_type 文件类型，应在注册的文件类型中
	 */
	void read(const std::filesystem::path& path, const string& file_type, const std::vector<std::any>& args);
	/**
	 * @brief 系统的写模型接口
	 * @param model 模型id
	 * @param path 写出路径
	 * @param file_type 文件类型，应在注册的文件类型中
	 */
	void write(Index model, const std::filesystem::path& path, const string& file_type, const std::vector<std::any>& args);
	/**
	 * @brief 系统的功能注册函数
	 * @param handler 待注册的处理功能
	 */
	void registerHandler(std::unique_ptr<ModelIOHandler> handler);
    /**
     * @brief 注册的文件类型
     * @return 键是文件类型，值是支持的文件扩展名列表(如"txt", "obj")
     */
    const unordered_map<string, vector<string>>& registeredFileTypes();

private:
    ModelManager* manager_;
    unordered_map<string, unique_ptr<ModelIOHandler>> handlers_;
    unordered_map<string, vector<string>> fileExtensions_;
};
}
