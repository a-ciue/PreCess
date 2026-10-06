#ifndef MODEL_PAYLOAD_H
#define MODEL_PAYLOAD_H

#include "ComponentData.h"
#include <filesystem>
#include <string>

struct ModelPayload {
    //! 模型名：UTF-8 字节串（char8_t 类型即编码标注；填充直用 path.u8string() 免往返转换）
    std::u8string model_name;
    ComponentDatas components;
};

/**
 * @brief std::u8string（UTF-8 字节）→ std::string（同字节 reinterpret，中文名安全）
 * @note 消费侧转换：模型层 addModel 生态仍为 std::string，仅在载荷出入口各转一次
 */
inline std::string u8Narrow(const std::u8string& s)
{
    return { reinterpret_cast<const char*>(s.data()), s.size() };
}

/**
 * @brief fs::path → UTF-8 std::string（保留字节语义，中文路径安全）
 * @note C++20 起 path::u8string() 返回 std::u8string——载荷填充已直用原生 u8string；
 *       本 helper 服务仍需窄串的场景（OCCT/文件流 API 等）
 */
inline std::string pathUtf8(const std::filesystem::path& path)
{
    const std::u8string u8 = path.u8string();
    return { reinterpret_cast<const char*>(u8.data()), u8.size() };
}

#endif // MODEL_PAYLOAD_H
