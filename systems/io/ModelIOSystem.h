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
#include "ModelIOHandler.h"

class ModelManager;

namespace systems::io {
using std::string;
using std::unordered_map;
using std::vector;

/**
 * @brief 对应Handler的元信息
 */
struct HandlerMetaData {
    string file_type; //> 处理的文件格式，取Wikipedia上对应模型类型词条名称，如"Wavefront .obj file", "ISO 10303-21", "STL (file format)"
    vector<string> extensions; //> 支持的文件扩展名列表，如["txt", "obj"]
};

/**
 * @brief 模型IO系统，每种文件格式只能注册一个处理器
 */
class ModelIOSystem {
public:
    using Handler = ModelIOHandler;
    static const string name; //> 系统名称

    ModelIOSystem(ModelManager& manager);
    /**
     * @brief 系统的读模型接口
     * @param path 读取路径
     * @param file_type 文件类型，应在注册的文件类型中
     * @param args 读操作的参数，传给Handler
     */
    void read(const std::filesystem::path& path, const string& file_type, const std::vector<std::any>& args);
    /**
     * @brief 系统的写模型接口
     * @param model 模型id
     * @param path 写出路径
     * @param file_type 文件类型，应在注册的文件类型中
     * @param args 写操作的参数，传给Handler
     */
    void write(Index model, const std::filesystem::path& path, const string& file_type, const std::vector<std::any>& args);
    /**
     * @brief 系统的功能Handler注册函数
     * @param meta_data 功能的元信息
     * @param handler 待注册的处理功能
     */
    bool registerHandler(const HandlerMetaData& meta_data, std::shared_ptr<ModelIOHandler> handler);
    /**
     * @brief 系统功能注销
     */
    void unregisterHandler(const HandlerMetaData& meta_data);
    /**
     * @brief 注册的文件类型
     * @return 键是文件类型，值是支持的文件扩展名列表(如"txt", "obj")
     */
    const unordered_map<string, vector<string>>& registeredFileTypes();

private:
    ModelManager* manager_;
    unordered_map<string, std::shared_ptr<ModelIOHandler>> handlers_; //> 键是文件类型
    unordered_map<string, vector<string>> fileExtensions_; //> 键是文件类型，值是支持的文件扩展名列表(如"txt", "obj")
};
}
