/**
 * @file VtkXmlReader.cpp
 * @brief VTK XML 文件读取实现：手写 XML 解析、base64 与 DataArray 数值读取
 *
 * 解析器只覆盖 VTK XML 用到的子集（元素/属性/文本/CDATA/注释/XML 声明），
 * 其余一律拒绝：DOCTYPE 与 ENTITY 声明直接报错（防外部实体注入），实体
 * 引用只接受五个预定义转义与数字字符引用，不解析任何外部资源。
 *
 * DataArray 三种承载：format="ascii" 逐 token 解析；format="binary" 为
 * base64 编码的 [长度前缀 + 数据]（老写手前后两段分开编码，新写手连续
 * 编码，两种布局都兼容）；format="appended" 从 AppendedData 段按 offset
 * 取连续编码流。二进制仅支持小端。
 */
#include "VtkXmlReader.h"

#include <cstring>
#include <fstream>
#include <sstream>

namespace vtkxml {

namespace {

    //! @brief 去掉字符两端的空白
    char skipWhitespace(const std::string& text, size_t& pos)
    {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
            ++pos;
        }
        return pos < text.size() ? text[pos] : '\0';
    }

    //! @brief 解码实体引用：仅五个预定义转义与数字字符引用，其余拒绝
    char decodeEntity(const std::string& text, size_t& pos)
    {
        // pos 指向 '&' 之后；实体以 ';' 结束
        const size_t begin = pos;
        const size_t end = text.find(';', pos);
        if (end == std::string::npos || end == begin || end - begin > 10) {
            throw ReadError("malformed entity reference");
        }
        const std::string entity = text.substr(begin, end - begin);
        pos = end + 1;
        if (entity == "lt") {
            return '<';
        }
        if (entity == "gt") {
            return '>';
        }
        if (entity == "amp") {
            return '&';
        }
        if (entity == "quot") {
            return '"';
        }
        if (entity == "apos") {
            return '\'';
        }
        if (entity[0] == '#') {
            const bool hex = entity.size() > 2 && (entity[1] == 'x' || entity[1] == 'X');
            try {
                const long code = std::stol(entity.substr(hex ? 2 : 1), nullptr, hex ? 16 : 10);
                if (code <= 0 || code > 255) {
                    throw std::out_of_range("code");
                }
                return static_cast<char>(code);
            } catch (const std::exception&) {
                throw ReadError("bad numeric character reference '&" + entity + ";'");
            }
        }
        throw ReadError("unsupported entity reference '&" + entity + ";'");
    }

    //! @brief 解析文本直到 '<'：解码实体引用后拼接
    std::string parseText(const std::string& text, size_t& pos)
    {
        std::string result;
        while (pos < text.size() && text[pos] != '<') {
            if (text[pos] == '&') {
                ++pos;
                result.push_back(decodeEntity(text, pos));
            } else {
                result.push_back(text[pos]);
                ++pos;
            }
        }
        return result;
    }

    //! @brief 解析属性值（引号包裹），解码实体引用
    std::string parseAttributeValue(const std::string& text, size_t& pos)
    {
        const char quote = text[pos];
        ++pos;
        std::string result;
        while (pos < text.size() && text[pos] != quote) {
            if (text[pos] == '&') {
                ++pos;
                result.push_back(decodeEntity(text, pos));
            } else {
                result.push_back(text[pos]);
                ++pos;
            }
        }
        if (pos >= text.size()) {
            throw ReadError("unterminated attribute value");
        }
        ++pos; // 消费收尾引号
        return result;
    }

    //! @brief XML 元素嵌套深度上限：VTK XML 实际嵌套远小于此，防不可信输入的栈溢出
    constexpr int kMaxXmlNestingDepth = 64;

    //! @brief 解析一个元素：开标签（属性）、子内容与闭标签
    XmlNode parseElement(const std::string& text, size_t& pos, int depth)
    {
        if (depth > kMaxXmlNestingDepth) {
            throw ReadError("XML nesting too deep");
        }
        ++pos; // 消费 '<'
        XmlNode node;
        size_t end = text.find_first_of(" \t\r\n/>", pos);
        if (end == std::string::npos) {
            throw ReadError("unterminated start tag");
        }
        node.name = text.substr(pos, end - pos);
        pos = end;

        // 属性表直到 '>' 或自闭合 '/>'；'/' 后必须紧跟 '>'
        bool self_closing = false;
        while (pos < text.size() && text[pos] != '>') {
            if (std::isspace(static_cast<unsigned char>(text[pos]))) {
                ++pos;
                continue;
            }
            if (text[pos] == '/') {
                self_closing = true;
                ++pos;
                if (pos >= text.size() || text[pos] != '>') {
                    throw ReadError("expected '>' after '/' in tag <" + node.name + ">");
                }
                break;
            }
            const size_t name_begin = pos;
            while (pos < text.size() && text[pos] != '='
                && !std::isspace(static_cast<unsigned char>(text[pos]))) {
                ++pos;
            }
            const std::string attr_name = text.substr(name_begin, pos - name_begin);
            if (attr_name.empty()) {
                throw ReadError("empty attribute name in tag <" + node.name + ">");
            }
            skipWhitespace(text, pos);
            if (pos >= text.size() || text[pos] != '=') {
                throw ReadError("attribute '" + attr_name + "' has no value in tag <" + node.name + ">");
            }
            ++pos;
            skipWhitespace(text, pos);
            if (pos >= text.size() || (text[pos] != '"' && text[pos] != '\'')) {
                throw ReadError("attribute '" + attr_name + "' value is not quoted");
            }
            node.attributes.emplace(attr_name, parseAttributeValue(text, pos));
        }
        if (pos >= text.size()) {
            throw ReadError("unterminated tag <" + node.name + ">");
        }
        ++pos; // 消费 '>'
        if (self_closing) {
            return node;
        }

        // 子内容：文本 / CDATA / 注释 / PI / 子元素，直到配对的闭标签
        std::string text_buffer;
        while (true) {
            if (pos >= text.size()) {
                throw ReadError("missing end tag </" + node.name + ">");
            }
            if (text[pos] != '<') {
                text_buffer += parseText(text, pos);
                continue;
            }

            if (text.compare(pos, 4, "<!--") == 0) {
                const size_t comment_end = text.find("-->", pos + 4);
                if (comment_end == std::string::npos) {
                    throw ReadError("unterminated comment");
                }
                pos = comment_end + 3;
            } else if (text.compare(pos, 9, "<![CDATA[") == 0) {
                const size_t cdata_end = text.find("]]>", pos + 9);
                if (cdata_end == std::string::npos) {
                    throw ReadError("unterminated CDATA section");
                }
                text_buffer += text.substr(pos + 9, cdata_end - pos - 9);
                pos = cdata_end + 3;
            } else if (text.compare(pos, 2, "<?") == 0) {
                const size_t pi_end = text.find("?>", pos + 2);
                if (pi_end == std::string::npos) {
                    throw ReadError("unterminated processing instruction");
                }
                pos = pi_end + 2;
            } else if (text.compare(pos, 2, "<!") == 0) {
                // 安全约束：DOCTYPE / ENTITY 及任何其余声明一律拒绝，不解析外部实体
                throw ReadError("markup declaration starting with '<!' is rejected (no DOCTYPE/ENTITY)");
            } else if (text.compare(pos, 2, "</") == 0) {
                const size_t tag_end = text.find('>', pos);
                if (tag_end == std::string::npos) {
                    throw ReadError("unterminated end tag");
                }
                const std::string end_name = text.substr(pos + 2, tag_end - pos - 2);
                size_t trim = end_name.find_first_not_of(" \t\r\n");
                const std::string trimmed = trim == std::string::npos
                    ? std::string {}
                    : end_name.substr(trim, end_name.find_last_not_of(" \t\r\n") - trim + 1);
                if (trimmed != node.name) {
                    throw ReadError("mismatched end tag </" + trimmed + "> for <" + node.name + ">");
                }
                pos = tag_end + 1;
                break;
            } else {
                node.children.push_back(parseElement(text, pos, depth + 1));
            }
        }
        node.text = std::move(text_buffer);
        return node;
    }

    //! @brief base64 表字符值，非法字符返回 -1
    int base64Value(char c)
    {
        if (c >= 'A' && c <= 'Z') {
            return c - 'A';
        }
        if (c >= 'a' && c <= 'z') {
            return c - 'a' + 26;
        }
        if (c >= '0' && c <= '9') {
            return c - '0' + 52;
        }
        if (c == '+') {
            return 62;
        }
        if (c == '/') {
            return 63;
        }
        return -1;
    }

    //! @brief 从 DataArray 元素属性取 format，缺省 ascii
    std::string arrayFormat(const XmlNode& array)
    {
        const std::string* format = array.attribute("format");
        return format ? *format : "ascii";
    }

    //! @brief DataArray 的二进制元素宽度（字节），按 type 属性
    size_t elementTypeSize(const std::string& type)
    {
        if (type == "Int8" || type == "UInt8") {
            return 1;
        }
        if (type == "Int16" || type == "UInt16") {
            return 2;
        }
        if (type == "Int32" || type == "UInt32" || type == "Float32") {
            return 4;
        }
        if (type == "Int64" || type == "UInt64" || type == "Float64") {
            return 8;
        }
        throw ReadError("unsupported DataArray type '" + type + "'");
    }

    //! @brief 把 base64 字符流解码为字节（跳过空白，padding 结束数据段）
    std::vector<uint8_t> decodeStream(const std::string& stream, size_t begin)
    {
        std::vector<uint8_t> bytes;
        bytes.reserve((stream.size() - begin) / 4 * 3);
        uint32_t group = 0;
        int digits = 0;
        for (size_t i = begin; i < stream.size(); ++i) {
            const char c = stream[i];
            if (std::isspace(static_cast<unsigned char>(c))) {
                continue;
            }
            if (c == '=') {
                break; // padding：数据段结束
            }
            const int value = base64Value(c);
            if (value < 0) {
                throw ReadError("invalid base64 character in DataArray");
            }
            group = (group << 6) | static_cast<uint32_t>(value);
            if (++digits == 4) {
                bytes.push_back(static_cast<uint8_t>(group >> 16));
                bytes.push_back(static_cast<uint8_t>(group >> 8));
                bytes.push_back(static_cast<uint8_t>(group));
                group = 0;
                digits = 0;
            }
        }
        // 尾组余 1 个合法字符属于非法编码，余 2/3 个字符分别补 1/2 字节
        if (digits == 1) {
            throw ReadError("truncated base64 group in DataArray");
        }
        if (digits == 2) {
            bytes.push_back(static_cast<uint8_t>(group >> 4));
        } else if (digits == 3) {
            bytes.push_back(static_cast<uint8_t>(group >> 10));
            bytes.push_back(static_cast<uint8_t>(group >> 2));
        }
        return bytes;
    }

    //! @brief 小端读取二进制标量
    template <typename T>
    T readLittleEndian(const uint8_t* bytes)
    {
        T value = 0;
        std::memcpy(&value, bytes, sizeof(T));
        return value;
    }

    /**
     * @brief 解析 DataArray 的二进制承载，返回原始数据字节
     *
     * inline binary 兼容两种历史布局：长度前缀与数据分开编码（VTK 传统写出）
     * 或连续编码（与 appended 相同）；按前缀长度自洽性自动选择。
     */
    std::vector<uint8_t> decodeBinary(const std::string& encoded, size_t header_size, bool separate_header_layout)
    {
        if (separate_header_layout) {
            // 前缀单独编码：其 base64 段自带 padding，先解码出前缀长度；
            // 解码结果短于 header_size 时拒绝，避免 memcpy 越界读
            const std::vector<uint8_t> header = decodeStream(encoded, 0);
            if (header.size() < header_size) {
                throw ReadError("binary DataArray prefix shorter than header size");
            }
            size_t prefix = 0;
            std::memcpy(&prefix, header.data(), header_size);
            // 数据段独立编码：从 padding 之后的下一字符开始
            std::vector<uint8_t> data;
            for (size_t i = 0; i < encoded.size(); ++i) {
                if (encoded[i] == '=') {
                    // 前缀段的 padding 结束位置之后即数据段起点
                    size_t next = encoded.find_first_not_of("= \t\r\n", i);
                    if (next == std::string::npos) {
                        throw ReadError("binary DataArray has no payload after prefix");
                    }
                    data = decodeStream(encoded, next);
                    break;
                }
            }
            if (data.size() < prefix) {
                throw ReadError("binary DataArray payload shorter than declared length");
            }
            data.resize(prefix);
            return data;
        }

        // 连续布局：前缀与数据一体编码
        const std::vector<uint8_t> stream = decodeStream(encoded, 0);
        if (stream.size() < header_size) {
            throw ReadError("binary DataArray too short for length prefix");
        }
        size_t prefix = 0;
        std::memcpy(&prefix, stream.data(), header_size);
        if (stream.size() < header_size + prefix) {
            throw ReadError("binary DataArray payload shorter than declared length");
        }
        return std::vector<uint8_t>(stream.begin() + static_cast<ptrdiff_t>(header_size),
            stream.begin() + static_cast<ptrdiff_t>(header_size + prefix));
    }

    /**
     * @brief 按承载方式取 DataArray 的原始字节并按元素类型展开为数值
     *
     * @tparam T 目标数值类型（double / int64_t），ascii 逐 token 解析、
     *         binary 按声明类型逐元素换算
     */
    template <typename T>
    std::vector<T> decodeNumbers(const XmlNode& array, const XmlDocument& doc)
    {
        const std::string format = arrayFormat(array);
        const std::string type = array.attribute("type") ? *array.attribute("type") : "Float64";
        const size_t element_size = elementTypeSize(type);

        std::vector<T> values;
        if (format == "ascii") {
            std::istringstream input(array.text);
            T value {};
            while (input >> value) {
                values.push_back(value);
            }
            if (!input.eof()) {
                throw ReadError("DataArray '" + (array.attribute("Name") ? *array.attribute("Name") : std::string("?"))
                    + "' has malformed ascii number");
            }
            return values;
        }

        if (const std::string* order = array.attribute("byte_order"); order && *order != "LittleEndian") {
            throw ReadError("only LittleEndian binary DataArray is supported");
        }

        std::vector<uint8_t> bytes;
        if (format == "binary") {
            // 优先按"前缀单独编码"尝试，长度不自洽再按"连续编码"解释
            try {
                bytes = decodeBinary(array.text, doc.headerSize(), true);
            } catch (const ReadError&) {
                bytes = decodeBinary(array.text, doc.headerSize(), false);
            }
        } else if (format == "appended") {
            const std::string* offset_text = array.attribute("offset");
            if (!offset_text || (doc.appendedData().empty() && doc.appendedBytes().empty())) {
                throw ReadError("appended DataArray requires offset and AppendedData section");
            }
            size_t offset = 0;
            try {
                offset = static_cast<size_t>(std::stoull(*offset_text));
            } catch (const std::exception&) {
                throw ReadError("bad appended offset '" + *offset_text + "'");
            }
            if (doc.appendedRaw()) {
                // raw 承载：offset 为 '_' 后的字节偏移，块布局 = 长度前缀 + 数据；
                // 边界检查用减法形式，offset 接近 size_t 上限时加法会回绕绕过检查
                const std::vector<uint8_t>& stream = doc.appendedBytes();
                if (offset > stream.size() || stream.size() - offset < doc.headerSize()) {
                    throw ReadError("appended offset out of range");
                }
                size_t prefix = 0;
                std::memcpy(&prefix, stream.data() + offset, doc.headerSize());
                if (prefix > stream.size() - offset - doc.headerSize()) {
                    throw ReadError("raw appended block shorter than declared length");
                }
                bytes.assign(stream.begin() + static_cast<ptrdiff_t>(offset + doc.headerSize()),
                    stream.begin() + static_cast<ptrdiff_t>(offset + doc.headerSize() + prefix));
            } else {
                if (offset >= doc.appendedData().size()) {
                    throw ReadError("appended offset out of range");
                }
                bytes = decodeBinary(doc.appendedData().substr(offset), doc.headerSize(), false);
            }
        } else {
            throw ReadError("unsupported DataArray format '" + format + "'");
        }

        if (bytes.size() % element_size != 0) {
            throw ReadError("binary DataArray byte count not a multiple of element size");
        }
        values.reserve(bytes.size() / element_size);
        for (size_t i = 0; i < bytes.size(); i += element_size) {
            switch (element_size) {
            case 1:
                values.push_back(static_cast<T>(bytes[i]));
                break;
            case 2:
                values.push_back(readLittleEndian<uint16_t>(&bytes[i]));
                break;
            case 4:
                if (type == "Float32") {
                    values.push_back(readLittleEndian<float>(&bytes[i]));
                } else {
                    values.push_back(readLittleEndian<uint32_t>(&bytes[i]));
                }
                break;
            default:
                if (type == "Float64") {
                    values.push_back(readLittleEndian<double>(&bytes[i]));
                } else {
                    values.push_back(readLittleEndian<uint64_t>(&bytes[i]));
                }
                break;
            }
        }
        return values;
    }

} // namespace

const XmlNode* XmlNode::child(const std::string& child_name) const
{
    for (const auto& node : children) {
        if (node.name == child_name) {
            return &node;
        }
    }
    return nullptr;
}

const std::string* XmlNode::attribute(const std::string& key) const
{
    const auto it = attributes.find(key);
    return it == attributes.end() ? nullptr : &it->second;
}

std::vector<uint8_t> base64Decode(const std::string& text)
{
    return decodeStream(text, 0);
}

XmlDocument XmlDocument::load(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw ReadError("failed to open file '" + path.string() + "'");
    }
    const std::string content((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());

    XmlDocument doc;
    std::string parse_content = content;

    // encoding="raw" 的 AppendedData 段是原始二进制字节流（VTK 9 / ParaView 新版
    // 默认承载），其中可出现任意字节（含 '<'、'?'），不能作为 XML 文本解析，
    // 必须在解析前整段剥出：定位 <AppendedData ...> 标签后的 '_'，数据区取到
    // 最后一处闭合标签为止（用 rfind，数据内可能巧合出现同字节序列）
    const size_t appended_tag = parse_content.find("<AppendedData");
    if (appended_tag != std::string::npos) {
        const size_t tag_end = parse_content.find('>', appended_tag);
        if (tag_end == std::string::npos) {
            throw ReadError("unterminated AppendedData tag");
        }
        const std::string tag_text = parse_content.substr(appended_tag, tag_end - appended_tag);
        if (tag_text.find("encoding=\"raw\"") != std::string::npos) {
            const size_t underscore = parse_content.find('_', tag_end + 1);
            if (underscore == std::string::npos) {
                throw ReadError("raw AppendedData section has no leading '_'");
            }
            const size_t close = parse_content.rfind("</AppendedData>");
            const size_t data_end = close == std::string::npos ? parse_content.size() : close;
            if (data_end <= underscore + 1) {
                throw ReadError("raw AppendedData section is empty");
            }
            doc.appended_bytes_.assign(
                parse_content.begin() + static_cast<ptrdiff_t>(underscore + 1),
                parse_content.begin() + static_cast<ptrdiff_t>(data_end));
            doc.appended_raw_ = true;
            // 从解析内容中移除二进制区（保留 '_' 与闭合标签，维持文档结构）
            parse_content.erase(underscore + 1, data_end - underscore - 1);
        }
    }

    size_t pos = 0;
    if (skipWhitespace(parse_content, pos) != '<') {
        throw ReadError("document does not start with '<'");
    }
    // 文档级 XML 声明（<?xml ...?>）不是元素，跳过后根元素必须紧跟
    if (parse_content.compare(pos, 2, "<?") == 0) {
        const size_t declaration_end = parse_content.find("?>", pos + 2);
        if (declaration_end == std::string::npos) {
            throw ReadError("unterminated XML declaration");
        }
        pos = declaration_end + 2;
        if (skipWhitespace(parse_content, pos) != '<') {
            throw ReadError("no root element after XML declaration");
        }
    }
    doc.root_ = parseElement(parse_content, pos, 0);

    const std::string* header_type = doc.root_.attribute("header_type");
    if (header_type && *header_type == "UInt64") {
        doc.header_size_ = 8;
    }
    // 压缩数据（VTKFile 的 compressor 属性，zlib/lz4/lzma）本读取器不解压，
    // 显式报错避免按普通字节解释出静默错数据
    if (doc.root_.attribute("compressor")) {
        throw ReadError("compressed VTK XML data is not supported, re-export without compressor");
    }

    // AppendedData 的文本形如 "_<base64 流>"：去掉前导 '_'，结尾空白由解码器跳过
    if (const XmlNode* appended = doc.root_.child("AppendedData")) {
        const std::string& raw = appended->text;
        const size_t underscore = raw.find('_');
        if (underscore == std::string::npos) {
            throw ReadError("AppendedData section has no leading '_'");
        }
        doc.appended_ = raw.substr(underscore + 1);
    }
    return doc;
}

std::vector<double> readDoubles(const XmlNode& array, const XmlDocument& doc)
{
    return decodeNumbers<double>(array, doc);
}

std::vector<int64_t> readIntegers(const XmlNode& array, const XmlDocument& doc)
{
    return decodeNumbers<int64_t>(array, doc);
}

}
