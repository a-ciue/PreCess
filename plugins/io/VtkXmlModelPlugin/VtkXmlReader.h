/**
 * @file VtkXmlReader.h
 * @brief 面向 VTK XML 网格文件的轻量读取器：XML 解析、base64 与 DataArray 数值读取
 */
#ifndef VTK_XML_READER_H
#define VTK_XML_READER_H

#include <cstdint>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace vtkxml {

/**
 * @brief XML 元素节点：名字、属性、子元素与直接文本
 *
 * 只覆盖 VTK XML 用到的特性（元素/属性/文本/CDATA），非通用 XML 实现。
 */
struct XmlNode {
    std::string name;
    std::map<std::string, std::string> attributes;
    std::vector<XmlNode> children;
    std::string text;

    //! @brief 取第一个同名子元素，不存在返回 nullptr
    const XmlNode* child(const std::string& child_name) const;
    //! @brief 取属性值，不存在返回 nullptr
    const std::string* attribute(const std::string& key) const;
};

//! @brief 读取失败（格式损坏、越界引用、不支持的能力）统一抛出
struct ReadError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

/**
 * @brief 一份 VTK XML 文档：根元素与 AppendedData 段信息
 *
 * 安全约定：解析前拒绝 DOCTYPE / ENTITY 声明，实体只支持五个预定义
 * 转义与数字字符引用，不加载任何外部资源。
 */
class XmlDocument {
public:
    /**
     * @brief 从文件加载并解析文档
     * @throw ReadError 文件不可读或 XML 结构非法
     */
    static XmlDocument load(const std::filesystem::path& path);

    const XmlNode& root() const { return root_; }
    //! @brief AppendedData 段的 base64 字符流（不含前导 '_' 与结尾空白），无该段为空
    const std::string& appendedData() const { return appended_; }
    //! @brief AppendedData 段的原始字节（encoding="raw"），offset 按字节相对 '_' 后计
    const std::vector<uint8_t>& appendedBytes() const { return appended_bytes_; }
    //! @brief AppendedData 是否为 encoding="raw" 的原始二进制承载
    bool appendedRaw() const { return appended_raw_; }
    //! @brief 二进制数据长度前缀的字节数（header_type：UInt32 -> 4，UInt64 -> 8）
    size_t headerSize() const { return header_size_; }

private:
    XmlNode root_;
    std::string appended_;
    std::vector<uint8_t> appended_bytes_;
    bool appended_raw_ { false };
    size_t header_size_ { 4 };
};

//! @brief base64 解码：容忍空白与 padding，非法字符抛 ReadError
std::vector<uint8_t> base64Decode(const std::string& text);

//! @brief 读 DataArray 为 double 序列（Points 坐标等浮点数据）
std::vector<double> readDoubles(const XmlNode& array, const XmlDocument& doc);

//! @brief 读 DataArray 为 int64 序列（connectivity/offsets/types 等整数数据）
std::vector<int64_t> readIntegers(const XmlNode& array, const XmlDocument& doc);

}
#endif // !VTK_XML_READER_H
