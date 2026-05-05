#ifndef SYSTEMS_IO_MODEL_IO_SYSTEM_BASE_H
#define SYSTEMS_IO_MODEL_IO_SYSTEM_BASE_H

#include "Core.h"
#include <filesystem>
#include <any>

namespace systems::io {
/**
 * @brief ModelIOSystem接口基类，出于插件开发而设计的接口基类。
 * 
 * 目前只被插件Handler使用。
 * 如果基于接口解耦的目的，将出现ModelIOSystem地方全使用基类接口，要参考派生类补全接口基类的函数声明
 * @sa systems::io::ModelIOSystem
 * @sa systems::algo::HandlerContext
 */
class ModelIOSystemBase {
public:
    virtual ~ModelIOSystemBase() = default;
    virtual void read(const std::filesystem::path& path, const std::string& file_type, const std::vector<std::any>& args) = 0;
    virtual void write(Index model, const std::filesystem::path& path, const std::string& file_type, const std::vector<std::any>& args) = 0;
    virtual void writeComponents(const std::vector<Index>& component_ids,
        const std::filesystem::path& path,
        const std::string& file_type,
        const std::vector<std::any>& args)
        = 0;
};
}

#endif