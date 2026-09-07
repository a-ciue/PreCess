/**
 * @file OffMeshIO.cpp
 * @brief OFF 文件读写实现
 */
#include "OffMeshIO.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

namespace {

//! @brief ASCII OFF 头部的坐标布局
struct AsciiLayout {
    int coords_per_vertex { 3 }; //!< 每条顶点记录开头的坐标分量个数
    bool dimension_in_counts { false }; //!< 维度写在计数行上（nOFF / 4nOFF）
    bool has_homogeneous { false }; //!< 4OFF / 4nOFF 的齐次分量 w
};

//! @brief 去掉行首尾空白与行尾 '\r'（二进制模式下 CRLF 不做转换）
std::string trimLine(const std::string& line)
{
    const auto first = line.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = line.find_last_not_of(" \t\r\n");
    return line.substr(first, last - first + 1);
}

std::string toUpper(std::string text)
{
    // 只用于 OFF 关键字这类 ASCII 文本；std::toupper 需转 unsigned char 避免负值入参的未定义行为
    std::transform(text.begin(), text.end(), text.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

/**
 * @brief 取下一条有效记录行，跳过空行与 '#' 注释行
 * @return 读到有效行返回 true，到文件尾返回 false
 */
bool nextRecordLine(std::istream& input, std::string& line)
{
    std::string raw;
    while (std::getline(input, raw)) {
        line = trimLine(raw);
        if (!line.empty() && line.front() != '#') {
            return true;
        }
    }
    return false;
}

//! @brief 关键字前缀允许出现的规范标志（Geomview OFF），各至多出现一次
const std::string kOffKeywordFlags = "4nNCST";

/**
 * @brief 解析头部关键字，确定每条顶点记录的坐标分量个数
 *
 * 关键字形如 [标志]OFF：后缀判定不区分大小写，标志按原大小写判定（大小写语义不同）：
 * - '4'：顶点记录带齐次分量 w，坐标分量多一个（4OFF / 4nOFF）
 * - 'n'：维度写在计数行的首个数字上（nOFF / 4nOFF），故 'n' 未必是前缀首字符
 * - 'N' 顶点法向、'C' 颜色、'S'/'T' 纹理坐标：均为行尾附加属性，读取时丢弃
 *
 * 标志必须全部取自 kOffKeywordFlags 且不重复；出现集合外字符（如非标准关键字
 * "AnythingOFF"）一律拒绝，避免按猜错的布局解析出无意义数据。
 */
bool parseKeyword(const std::string& keyword, AsciiLayout& layout)
{
    const std::string upper = toUpper(keyword);
    const std::string suffix = "OFF";
    if (upper.size() < suffix.size()
        || upper.compare(upper.size() - suffix.size(), suffix.size(), suffix) != 0) {
        return false;
    }

    const std::string prefix = keyword.substr(0, keyword.size() - suffix.size());
    for (char flag : prefix) {
        if (kOffKeywordFlags.find(flag) == std::string::npos
            || std::count(prefix.begin(), prefix.end(), flag) > 1) {
            return false;
        }
    }

    layout.has_homogeneous = prefix.find('4') != std::string::npos;
    layout.dimension_in_counts = prefix.find('n') != std::string::npos;
    layout.coords_per_vertex = 3 + (layout.has_homogeneous ? 1 : 0);
    return true;
}

//! @brief 读取小端 int32（二进制 OFF 的计数与索引）
bool readInt32LE(std::istream& input, int32_t& value)
{
    unsigned char bytes[4] {};
    input.read(reinterpret_cast<char*>(bytes), sizeof(bytes));
    if (input.gcount() != static_cast<std::streamsize>(sizeof(bytes))) {
        return false;
    }

    const uint32_t raw = static_cast<uint32_t>(bytes[0])
        | (static_cast<uint32_t>(bytes[1]) << 8)
        | (static_cast<uint32_t>(bytes[2]) << 16)
        | (static_cast<uint32_t>(bytes[3]) << 24);
    value = static_cast<int32_t>(raw);
    return true;
}

//! @brief 读取小端 float32（二进制 OFF 的坐标）
bool readFloat32LE(std::istream& input, float& value)
{
    int32_t bits = 0;
    if (!readInt32LE(input, bits)) {
        return false;
    }

    const uint32_t raw = static_cast<uint32_t>(bits);
    std::memcpy(&value, &raw, sizeof(value));
    return true;
}

//! @brief 读取 ASCII 主体：计数行之后是顶点记录，再之后是面记录
bool readAscii(std::istream& input, const AsciiLayout& layout, MeshData& mesh)
{
    std::string line;
    if (!nextRecordLine(input, line)) {
        spdlog::error("OffMeshIO: missing counts line");
        return false;
    }

    // 计数行：nOFF 系列首个数字是维度，其后依次是顶点数、面数与边数（边数不参与建模）
    std::istringstream counts(line);
    int coords_per_vertex = layout.coords_per_vertex;
    if (layout.dimension_in_counts) {
        int dimension = 0;
        if (!(counts >> dimension) || dimension < 3) {
            spdlog::error("OffMeshIO: invalid dimension in counts line '{}'", line);
            return false;
        }
        coords_per_vertex = dimension + (layout.has_homogeneous ? 1 : 0);
    }

    Index vertex_count = 0;
    Index face_count = 0;
    if (!(counts >> vertex_count) || vertex_count < 0) {
        spdlog::error("OffMeshIO: invalid vertex count in counts line '{}'", line);
        return false;
    }
    if (!(counts >> face_count) || face_count < 0) {
        spdlog::error("OffMeshIO: invalid face count in counts line '{}'", line);
        return false;
    }

    mesh.init();
    mesh.vertex_positions_.reserve(static_cast<size_t>(vertex_count));

    // 顶点记录：取前 3 个分量作为坐标，行尾的齐次分量与附加属性丢弃
    for (Index v = 0; v < vertex_count; ++v) {
        if (!nextRecordLine(input, line)) {
            spdlog::error("OffMeshIO: vertex records truncated at {} of {}", v, vertex_count);
            return false;
        }

        std::istringstream record(line);
        std::array<double, 3> position { 0.0, 0.0, 0.0 };
        for (int c = 0; c < 3; ++c) {
            if (!(record >> position[c])) {
                spdlog::error("OffMeshIO: bad vertex record '{}'", line);
                return false;
            }
        }
        mesh.vertex_positions_.push_back(position);

        // 校验声明的坐标分量个数够用，避免把属性数字误当坐标
        double ignored = 0.0;
        for (int c = 3; c < coords_per_vertex; ++c) {
            if (!(record >> ignored)) {
                spdlog::error("OffMeshIO: vertex record '{}' has fewer than {} components", line, coords_per_vertex);
                return false;
            }
        }
    }
    if (layout.has_homogeneous) {
        spdlog::warn("OffMeshIO: homogeneous component of 4OFF is ignored, coordinates are used as-is");
    }

    // 面记录：首个数字是该面的角点数，其后是角点的局部点索引，行尾附加属性丢弃
    Index skipped_faces = 0;
    for (Index f = 0; f < face_count; ++f) {
        if (!nextRecordLine(input, line)) {
            spdlog::error("OffMeshIO: face records truncated at {} of {}", f, face_count);
            return false;
        }

        std::istringstream record(line);
        Index corners = 0;
        if (!(record >> corners) || corners < 0) {
            spdlog::error("OffMeshIO: bad face record '{}'", line);
            return false;
        }
        if (corners < 3) {
            ++skipped_faces; // 少于 3 个点的记录无法作为面单元保留
            continue;
        }

        for (Index c = 0; c < corners; ++c) {
            Index point_id = 0;
            if (!(record >> point_id)) {
                spdlog::error("OffMeshIO: face record '{}' has fewer than {} corners", line, corners);
                return false;
            }
            if (point_id < 0 || point_id >= vertex_count) {
                spdlog::error("OffMeshIO: face point id {} out of range [0, {})", point_id, vertex_count);
                return false;
            }
            mesh.face_vertices_.push_back(point_id);
        }
        mesh.face_vertices_offset_.push_back(static_cast<Index>(mesh.face_vertices_.size()));
    }

    mesh.vertex_count_ = static_cast<Index>(mesh.vertex_positions_.size());
    if (skipped_faces > 0) {
        spdlog::warn("OffMeshIO: skipped {} face(s) with fewer than 3 corners", skipped_faces);
    }
    return true;
}

//! @brief 读取 Geomview 二进制主体：头部行已由调用方消费，此处从计数开始读
bool readBinary(std::istream& input, MeshData& mesh)
{
    int32_t vertex_count = 0;
    int32_t face_count = 0;
    int32_t edge_count = 0; // 边数不参与建模，仅占位读取
    if (!readInt32LE(input, vertex_count) || !readInt32LE(input, face_count)
        || !readInt32LE(input, edge_count)) {
        spdlog::error("OffMeshIO: truncated binary OFF header");
        return false;
    }
    if (vertex_count < 0 || face_count < 0) {
        spdlog::error("OffMeshIO: negative binary OFF counts, vertices={} faces={}", vertex_count, face_count);
        return false;
    }

    // 计数与剩余字节对账：头部损坏会给出天文数字，先拦下再做 reserve
    const std::streamoff data_begin = input.tellg();
    input.seekg(0, std::ios::end);
    const std::streamoff file_end = input.tellg();
    input.seekg(data_begin, std::ios::beg); // 回到数据起点（相对文件开头定位）
    if (data_begin < 0 || file_end < data_begin || !input) {
        spdlog::error("OffMeshIO: cannot locate binary OFF payload");
        return false;
    }
    const int64_t remaining = static_cast<int64_t>(file_end - data_begin);
    const int64_t min_bytes = static_cast<int64_t>(vertex_count) * 3 * 4 // 每点 3 个 float32
        + static_cast<int64_t>(face_count) * 4; // 每面至少 1 个 int32 角点数
    if (remaining < min_bytes) {
        spdlog::error("OffMeshIO: binary OFF counts exceed file size, need {} bytes but only {} left",
            min_bytes, remaining);
        return false;
    }

    mesh.init();
    mesh.vertex_positions_.reserve(static_cast<size_t>(vertex_count));

    for (int32_t v = 0; v < vertex_count; ++v) {
        std::array<double, 3> position { 0.0, 0.0, 0.0 };
        for (int c = 0; c < 3; ++c) {
            float coordinate = 0.0F;
            if (!readFloat32LE(input, coordinate)) {
                spdlog::error("OffMeshIO: binary vertex records truncated at {} of {}", v, vertex_count);
                return false;
            }
            position[static_cast<size_t>(c)] = static_cast<double>(coordinate);
        }
        mesh.vertex_positions_.push_back(position);
    }

    int32_t skipped_faces = 0;
    for (int32_t f = 0; f < face_count; ++f) {
        int32_t corners = 0;
        if (!readInt32LE(input, corners)) {
            spdlog::error("OffMeshIO: binary face records truncated at {} of {}", f, face_count);
            return false;
        }
        if (corners < 0) {
            spdlog::error("OffMeshIO: negative corner count {} in binary face {}", corners, f);
            return false;
        }
        if (corners < 3) {
            // 退化面：跳过其索引字节后继续
            input.seekg(static_cast<std::streamoff>(corners) * 4, std::ios::cur);
            ++skipped_faces;
            continue;
        }

        for (int32_t c = 0; c < corners; ++c) {
            int32_t point_id = 0;
            if (!readInt32LE(input, point_id)) {
                spdlog::error("OffMeshIO: binary face {} corners truncated at {}", f, c);
                return false;
            }
            if (point_id < 0 || point_id >= vertex_count) {
                spdlog::error("OffMeshIO: binary face point id {} out of range [0, {})", point_id, vertex_count);
                return false;
            }
            mesh.face_vertices_.push_back(static_cast<Index>(point_id));
        }
        mesh.face_vertices_offset_.push_back(static_cast<Index>(mesh.face_vertices_.size()));
    }

    mesh.vertex_count_ = static_cast<Index>(mesh.vertex_positions_.size());
    if (skipped_faces > 0) {
        spdlog::warn("OffMeshIO: skipped {} binary face(s) with fewer than 3 corners", skipped_faces);
    }
    return true;
}

/**
 * @brief 遍历 mesh 中可写出的面，回调收到该面在 face_vertices_ 中的区间 [begin, end)
 *
 * 跳过越界的脏面、点索引越界的面，以及角点数小于 3 的退化面。
 */
template <class FaceVisitor>
void forEachWritableFace(const MeshData& mesh, FaceVisitor&& visit)
{
    if (mesh.face_vertices_offset_.size() < 2) {
        return;
    }

    const Index face_count = static_cast<Index>(mesh.face_vertices_offset_.size() - 1);
    const Index corner_count = static_cast<Index>(mesh.face_vertices_.size());
    const Index point_count = static_cast<Index>(mesh.vertex_positions_.size());

    for (Index f = 0; f < face_count; ++f) {
        const Index begin = mesh.face_vertices_offset_[static_cast<size_t>(f)];
        const Index end = mesh.face_vertices_offset_[static_cast<size_t>(f) + 1];
        if (begin < 0 || end < begin || end > corner_count || end - begin < 3) {
            continue;
        }

        bool in_range = true;
        for (Index c = begin; c < end; ++c) {
            const Index point_id = mesh.face_vertices_[static_cast<size_t>(c)];
            if (point_id < 0 || point_id >= point_count) {
                in_range = false;
                break;
            }
        }
        if (!in_range) {
            continue;
        }

        visit(begin, end);
    }
}

} // namespace

bool OffMeshIO::read(const std::filesystem::path& path, MeshData& mesh)
{
    // 二进制打开：二进制分支要读原始字节，'\r' 由 trimLine 处理
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        spdlog::error("OffMeshIO: failed to open file '{}'", path.string());
        return false;
    }

    std::string header;
    if (!nextRecordLine(input, header)) {
        spdlog::error("OffMeshIO: empty OFF file '{}'", path.string());
        return false;
    }

    std::istringstream header_stream(header);
    std::string keyword;
    header_stream >> keyword;

    // "OFF BINARY" 走 Geomview 二进制分支
    std::string second_token;
    if ((header_stream >> second_token) && toUpper(second_token) == "BINARY") {
        return readBinary(input, mesh);
    }

    AsciiLayout layout;
    if (!parseKeyword(keyword, layout)) {
        spdlog::error("OffMeshIO: unsupported OFF header '{}' in file '{}'", header, path.string());
        return false;
    }

    return readAscii(input, layout, mesh);
}

bool OffMeshIO::write(const std::filesystem::path& path, const MeshData& mesh)
{
    Index face_count = 0;
    forEachWritableFace(mesh, [&face_count](Index, Index) { ++face_count; });
    if (face_count == 0) {
        spdlog::error("OffMeshIO: no writable face, skip writing '{}'", path.string());
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        spdlog::error("OffMeshIO: failed to open file '{}' for writing", path.string());
        return false;
    }

    // 计数行的边数恒为 0：OFF 只承载顶点与面
    const Index vertex_count = static_cast<Index>(mesh.vertex_positions_.size());
    output << "OFF\n"
           << vertex_count << ' ' << face_count << " 0\n";

    // 17 位有效数字保证 double 往返无损
    output << std::setprecision(17);
    for (const auto& position : mesh.vertex_positions_) {
        output << position[0] << ' ' << position[1] << ' ' << position[2] << '\n';
    }

    forEachWritableFace(mesh, [&output, &mesh](Index begin, Index end) {
        output << (end - begin);
        for (Index c = begin; c < end; ++c) {
            output << ' ' << mesh.face_vertices_[static_cast<size_t>(c)];
        }
        output << '\n';
    });

    output.flush();
    if (!output) {
        spdlog::error("OffMeshIO: failed to write file '{}'", path.string());
        return false;
    }

    spdlog::info("OffMeshIO: wrote OFF file '{}' ({} vertices, {} faces)",
        path.string(), vertex_count, face_count);
    return true;
}
