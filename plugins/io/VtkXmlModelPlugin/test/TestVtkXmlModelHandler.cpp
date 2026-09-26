/**
 * @file TestVtkXmlModelHandler.cpp
 * @brief VtkXmlModelHandler 的单元测试：ascii/binary/appended 解析、单元分发、
 *        安全约束（拒绝 DOCTYPE/ENTITY）、错误处理与读写往返
 */
#include "ComponentData.h"
#include "MeshData.h"
#include "ModelLayer.h"
#include "ModelPayload.h"
#include "TempFile.h"
#include "VtkXmlModelHandler.h"
#include "VtkXmlReader.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

//! @brief 写出一份测试用文本文件
void writeFile(const fs::path& path, const std::string& content)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.is_open());
    output << content;
}

//! @brief 测试用 base64 编码（与读取器的解码互逆，用于构造二进制载荷）
std::string base64Encode(const std::vector<uint8_t>& bytes)
{
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string text;
    uint32_t group = 0;
    int digits = 0;
    for (const uint8_t byte : bytes) {
        group = (group << 8) | byte;
        if (++digits == 3) {
            text.push_back(table[(group >> 18) & 0x3f]);
            text.push_back(table[(group >> 12) & 0x3f]);
            text.push_back(table[(group >> 6) & 0x3f]);
            text.push_back(table[group & 0x3f]);
            group = 0;
            digits = 0;
        }
    }
    if (digits == 1) {
        group <<= 8;
        text.push_back(table[(group >> 18) & 0x3f]);
        text.push_back(table[(group >> 12) & 0x3f]);
        text.append("==");
    } else if (digits == 2) {
        group <<= 8;
        text.push_back(table[(group >> 18) & 0x3f]);
        text.push_back(table[(group >> 12) & 0x3f]);
        text.push_back(table[(group >> 6) & 0x3f]);
        text.append("=");
    }
    return text;
}

//! @brief 构造小端字节流辅助
template <typename T>
void putBytes(std::vector<uint8_t>& bytes, const T& value)
{
    const auto* raw = reinterpret_cast<const uint8_t*>(&value);
    bytes.insert(bytes.end(), raw, raw + sizeof(T));
}

//! @brief 把 double 序列打包为 [UInt32 长度前缀 + 数据] 的字节流
std::vector<uint8_t> packDoubles(const std::vector<double>& values)
{
    std::vector<uint8_t> data;
    for (const double value : values) {
        putBytes(data, value);
    }
    std::vector<uint8_t> packed;
    const uint32_t size = static_cast<uint32_t>(data.size());
    putBytes(packed, size);
    packed.insert(packed.end(), data.begin(), data.end());
    return packed;
}

//! @brief ascii 版 .vtp：4 点正方形（2 三角形）+ 1 条对角折线
const char* kAsciiVtp = "<?xml version=\"1.0\"?>\n"
                        "<VTKFile type=\"PolyData\" version=\"0.1\" byte_order=\"LittleEndian\">\n"
                        "  <PolyData>\n"
                        "    <Piece NumberOfPoints=\"4\" NumberOfVerts=\"0\" NumberOfLines=\"1\" NumberOfStrips=\"0\" NumberOfPolys=\"2\">\n"
                        "      <Points>\n"
                        "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"ascii\">\n"
                        "          0 0 0 1 0 0 1 1 0 0 1 0\n"
                        "        </DataArray>\n"
                        "      </Points>\n"
                        "      <Lines>\n"
                        "        <DataArray type=\"Int64\" Name=\"connectivity\" format=\"ascii\">0 2</DataArray>\n"
                        "        <DataArray type=\"Int64\" Name=\"offsets\" format=\"ascii\">2</DataArray>\n"
                        "      </Lines>\n"
                        "      <Polys>\n"
                        "        <DataArray type=\"Int64\" Name=\"connectivity\" format=\"ascii\">0 1 2 0 2 3</DataArray>\n"
                        "        <DataArray type=\"Int64\" Name=\"offsets\" format=\"ascii\">3 6</DataArray>\n"
                        "      </Polys>\n"
                        "    </Piece>\n"
                        "  </PolyData>\n"
                        "</VTKFile>\n";

//! @brief ascii 版 .vtu：1 线段 + 1 三角形 + 1 四面体
const char* kAsciiVtu = "<?xml version=\"1.0\"?>\n"
                        "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\">\n"
                        "  <UnstructuredGrid>\n"
                        "    <Piece NumberOfPoints=\"5\" NumberOfCells=\"3\">\n"
                        "      <Points>\n"
                        "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"ascii\">\n"
                        "          0 0 0 1 0 0 1 1 0 0 1 0 0 0 1\n"
                        "        </DataArray>\n"
                        "      </Points>\n"
                        "      <Cells>\n"
                        "        <DataArray type=\"Int64\" Name=\"connectivity\" format=\"ascii\">0 1 0 1 2 0 1 2 3</DataArray>\n"
                        "        <DataArray type=\"Int64\" Name=\"offsets\" format=\"ascii\">2 5 9</DataArray>\n"
                        "        <DataArray type=\"UInt8\" Name=\"types\" format=\"ascii\">3 5 10</DataArray>\n"
                        "      </Cells>\n"
                        "    </Piece>\n"
                        "  </UnstructuredGrid>\n"
                        "</VTKFile>\n";

//! @brief 把网格作为单组件加入模型层，返回组件 id 列表
std::vector<Index> addMeshComponent(ModelLayer& layer, std::unique_ptr<MeshData> mesh)
{
    ComponentDatas components;
    auto component = std::make_unique<ComponentData>();
    component->id = -1;
    component->mesh = std::move(mesh);
    components.push_back(std::move(component));

    const Index model_id = layer.addModel("model", std::move(components));
    REQUIRE(model_id >= 0);
    const std::vector<Index> component_ids = layer.modelById(model_id)->componentIds();
    REQUIRE(!component_ids.empty());
    return component_ids;
}

//! @brief 单个四面体 + 三角形面 + 边的混合网格
MeshData makeMixedMesh()
{
    MeshData mesh;
    mesh.init();
    mesh.vertex_positions_ = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
        { 0.0, 0.0, 1.0 },
    };
    mesh.vertex_count_ = 4;
    mesh.edge_vertices_ = { 0, 1 };
    mesh.face_vertices_ = { 0, 1, 2 };
    mesh.face_vertices_offset_ = { 0, 3 };
    mesh.solid_types_ = { 10 }; // VTK_TETRA
    mesh.solid_vertices_ = { 0, 1, 2, 3 };
    mesh.solid_vertices_offset_ = { 0, 4 };
    mesh.solid_faces_offset_ = { 0 };
    return mesh;
}

/**
 * @brief 读回文件并返回 payload 本体
 *
 * 按值返回：组件由 unique_ptr 持有，返回裸指针会随局部 payload 析构而悬空。
 */
ModelPayload requireReadPayload(systems::io::VtkXmlModelHandler& handler, const fs::path& path)
{
    std::optional<ModelPayload> payload;
    REQUIRE_NOTHROW(payload = handler.read_model(path, {}));
    REQUIRE(payload.has_value());
    REQUIRE(!payload->components.empty());
    REQUIRE(payload->components.front()->mesh);
    return std::move(*payload);
}
} // namespace

TEST_CASE("VtkXmlModelHandler reads ascii vtp with lines and polys")
{
    systems::io::VtkXmlModelHandler handler;
    const fs::path input = core::TempFile::instance().path().string() + "_read.vtp";

    writeFile(input, kAsciiVtp);
    const ModelPayload payload = requireReadPayload(handler, input);
    const MeshData* mesh = payload.components.front()->mesh.get();

    REQUIRE(mesh->vertex_count_ == 4);
    REQUIRE(mesh->face_vertices_ == std::vector<Index> { 0, 1, 2, 0, 2, 3 });
    REQUIRE(mesh->face_vertices_offset_ == std::vector<Index> { 0, 3, 6 });
    REQUIRE(mesh->edge_vertices_ == std::vector<Index> { 0, 2 });
}

TEST_CASE("VtkXmlModelHandler reads ascii vtu and dispatches cell types")
{
    systems::io::VtkXmlModelHandler handler;
    const fs::path input = core::TempFile::instance().path().string() + "_read.vtu";

    writeFile(input, kAsciiVtu);
    const ModelPayload payload = requireReadPayload(handler, input);
    const MeshData* mesh = payload.components.front()->mesh.get();

    REQUIRE(mesh->vertex_count_ == 5);
    // 线段 (0,1)、三角形 (0,1,2)、四面体 (0,1,2,3) 各归其位
    REQUIRE(mesh->edge_vertices_ == std::vector<Index> { 0, 1 });
    REQUIRE(mesh->face_vertices_ == std::vector<Index> { 0, 1, 2 });
    REQUIRE(mesh->face_vertices_offset_ == std::vector<Index> { 0, 3 });
    REQUIRE(mesh->solid_types_ == std::vector<unsigned char> { 10 });
    REQUIRE(mesh->solid_vertices_ == std::vector<Index> { 0, 1, 2, 3 });
    REQUIRE(mesh->solid_vertices_offset_ == std::vector<Index> { 0, 4 });
}

TEST_CASE("VtkXmlReader decodes signed integers from binary DataArray")
{
    // 有符号整型（Int8/16/32/64）的二进制展开必须保留符号位：
    // 走无符号中间类型会把 -1 读成大正数
    const auto makeFile = [](const std::string& type_name, const std::string& encoded) {
        return "<VTKFile type=\"PolyData\" version=\"0.1\">\n"
               "  <PolyData>\n"
               "    <Piece>\n"
               "      <Points><DataArray type=\""
            + type_name + "\" Name=\"ids\" format=\"binary\">" + encoded + "</DataArray></Points>\n"
                                                                           "    </Piece>\n"
                                                                           "  </PolyData>\n"
                                                                           "</VTKFile>\n";
    };

    SECTION("Int32 with negative values")
    {
        std::vector<uint8_t> header;
        putBytes<uint32_t>(header, 12);
        std::vector<uint8_t> data;
        putBytes<int32_t>(data, -1);
        putBytes<int32_t>(data, 7);
        putBytes<int32_t>(data, -2147483648);

        const fs::path input = core::TempFile::instance().path().string() + "_int32.vtp";
        writeFile(input, makeFile("Int32", base64Encode(header) + base64Encode(data)));

        const vtkxml::XmlDocument doc = vtkxml::XmlDocument::load(input);
        const vtkxml::XmlNode* array = doc.root().child("PolyData")->child("Piece")->child("Points")->child("DataArray");
        REQUIRE(array != nullptr);
        REQUIRE(vtkxml::readIntegers(*array, doc) == std::vector<int64_t> { -1, 7, -2147483648LL });
    }

    SECTION("Int16 with negative values")
    {
        std::vector<uint8_t> header;
        putBytes<uint32_t>(header, 4);
        std::vector<uint8_t> data;
        putBytes<int16_t>(data, -2);
        putBytes<int16_t>(data, 300);

        const fs::path input = core::TempFile::instance().path().string() + "_int16.vtp";
        writeFile(input, makeFile("Int16", base64Encode(header) + base64Encode(data)));

        const vtkxml::XmlDocument doc = vtkxml::XmlDocument::load(input);
        const vtkxml::XmlNode* array = doc.root().child("PolyData")->child("Piece")->child("Points")->child("DataArray");
        REQUIRE(array != nullptr);
        REQUIRE(vtkxml::readIntegers(*array, doc) == std::vector<int64_t> { -2, 300 });
    }
}

TEST_CASE("VtkXmlModelHandler reads inline binary and appended vtp")
{
    systems::io::VtkXmlModelHandler handler;

    // 3 点共 9 个 double 的载荷，两种二进制承载共用
    const std::vector<double> coordinates {
        0.0,
        0.0,
        0.0,
        1.0,
        0.0,
        0.0,
        0.0,
        1.0,
        0.0,
    };
    const std::vector<uint8_t> packed = packDoubles(coordinates);
    const uint32_t prefix = static_cast<uint32_t>(packed.size() - 4);

    SECTION("inline binary with separate header encoding")
    {
        std::vector<uint8_t> header;
        putBytes(header, prefix);
        const std::string text = base64Encode(header)
            + base64Encode(std::vector<uint8_t>(packed.begin() + 4, packed.end()));

        const std::string content = "<VTKFile type=\"PolyData\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt32\">\n"
                                    "  <PolyData>\n"
                                    "    <Piece NumberOfPoints=\"3\" NumberOfPolys=\"1\">\n"
                                    "      <Points>\n"
                                    "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"binary\">"
            + text + "</DataArray>\n"
                     "      </Points>\n"
                     "      <Polys>\n"
                     "        <DataArray type=\"Int64\" Name=\"connectivity\" format=\"ascii\">0 1 2</DataArray>\n"
                     "        <DataArray type=\"Int64\" Name=\"offsets\" format=\"ascii\">3</DataArray>\n"
                     "      </Polys>\n"
                     "    </Piece>\n"
                     "  </PolyData>\n"
                     "</VTKFile>\n";

        const fs::path input = core::TempFile::instance().path().string() + "_inline.vtp";
        writeFile(input, content);
        const ModelPayload payload = requireReadPayload(handler, input);
        REQUIRE(payload.components.front()->mesh->vertex_count_ == 3);
    }

    SECTION("appended with continuous encoding")
    {
        const std::string content = "<VTKFile type=\"PolyData\" version=\"0.1\" byte_order=\"LittleEndian\" header_type=\"UInt32\">\n"
                                    "  <PolyData>\n"
                                    "    <Piece NumberOfPoints=\"3\" NumberOfPolys=\"1\">\n"
                                    "      <Points>\n"
                                    "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"appended\" offset=\"0\"/>\n"
                                    "      </Points>\n"
                                    "      <Polys>\n"
                                    "        <DataArray type=\"Int64\" Name=\"connectivity\" format=\"ascii\">0 1 2</DataArray>\n"
                                    "        <DataArray type=\"Int64\" Name=\"offsets\" format=\"ascii\">3</DataArray>\n"
                                    "      </Polys>\n"
                                    "    </Piece>\n"
                                    "  </PolyData>\n"
                                    "  <AppendedData encoding=\"base64\">_"
            + base64Encode(packed) + "\n</AppendedData>\n"
                                     "</VTKFile>\n";

        const fs::path input = core::TempFile::instance().path().string() + "_appended.vtp";
        writeFile(input, content);
        const ModelPayload payload = requireReadPayload(handler, input);
        const MeshData* mesh = payload.components.front()->mesh.get();
        REQUIRE(mesh->vertex_count_ == 3);
        REQUIRE(mesh->vertex_positions_[1] == std::array<double, 3> { 1.0, 0.0, 0.0 });
    }
}

TEST_CASE("VtkXmlModelHandler reads raw appended binary and point-only datasets")
{
    systems::io::VtkXmlModelHandler handler;

    SECTION("raw appended with UInt64 headers and byte offsets")
    {
        // 3 点 Float32 坐标（36 B，块 0）+ 1 个三角形 Int32 连通（12 B，块 1），
        // 每块布局 = UInt64 长度前缀 + 数据，offset 为 '_' 后的字节偏移
        std::vector<uint8_t> points_block;
        putBytes<uint64_t>(points_block, 36);
        for (const float value : { 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f }) {
            putBytes(points_block, value);
        }
        std::vector<uint8_t> connectivity_block;
        putBytes<uint64_t>(connectivity_block, 12);
        for (const int32_t value : { 0, 1, 2 }) {
            putBytes(connectivity_block, value);
        }
        const size_t second_offset = points_block.size();

        std::string content = "<VTKFile type=\"PolyData\" version=\"1.0\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n"
                              "  <PolyData>\n"
                              "    <Piece NumberOfPoints=\"3\" NumberOfVerts=\"0\" NumberOfLines=\"0\" NumberOfStrips=\"0\" NumberOfPolys=\"1\">\n"
                              "      <Points><DataArray type=\"Float32\" NumberOfComponents=\"3\" format=\"appended\" offset=\"0\"/></Points>\n"
                              "      <Polys><DataArray type=\"Int32\" Name=\"connectivity\" format=\"appended\" offset=\""
            + std::to_string(second_offset)
            + "\"/>\n"
              "        <DataArray type=\"Int32\" Name=\"offsets\" format=\"ascii\">3</DataArray></Polys>\n"
              "    </Piece>\n"
              "  </PolyData>\n"
              "  <AppendedData encoding=\"raw\">_";
        content.append(reinterpret_cast<const char*>(points_block.data()), points_block.size());
        content.append(reinterpret_cast<const char*>(connectivity_block.data()), connectivity_block.size());
        content.append("\n  </AppendedData>\n</VTKFile>\n");

        const fs::path input = core::TempFile::instance().path().string() + "_raw.vtp";
        writeFile(input, content);
        const ModelPayload payload = requireReadPayload(handler, input);
        const MeshData* mesh = payload.components.front()->mesh.get();
        REQUIRE(mesh->vertex_count_ == 3);
        REQUIRE(mesh->vertex_positions_[1] == std::array<double, 3> { 1.0, 0.0, 0.0 });
        REQUIRE(mesh->face_vertices_ == std::vector<Index> { 0, 1, 2 });
    }

    SECTION("verts-only point cloud is accepted as vertices")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_points.vtp";
        writeFile(input,
            "<VTKFile type=\"PolyData\" version=\"1.0\" byte_order=\"LittleEndian\">\n"
            "  <PolyData>\n"
            "    <Piece NumberOfPoints=\"2\" NumberOfVerts=\"2\" NumberOfLines=\"0\" NumberOfStrips=\"0\" NumberOfPolys=\"0\">\n"
            "      <Points><DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"ascii\">"
            "0 0 0 1 1 1</DataArray></Points>\n"
            "    </Piece>\n"
            "  </PolyData>\n"
            "</VTKFile>\n");

        const ModelPayload payload = requireReadPayload(handler, input);
        const MeshData* mesh = payload.components.front()->mesh.get();
        // 顶点即数据：无任何单元也读入
        REQUIRE(mesh->vertex_count_ == 2);
        REQUIRE(mesh->face_vertices_.empty());
        REQUIRE(mesh->solid_types_.empty());
    }

    SECTION("compressed dataset is rejected explicitly")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_zlib.vtp";
        writeFile(input,
            "<VTKFile type=\"PolyData\" version=\"1.0\" byte_order=\"LittleEndian\" compressor=\"vtkZLibDataCompressor\">\n"
            "  <PolyData/>\n"
            "</VTKFile>\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }
}

TEST_CASE("VtkXmlModelHandler rejects DOCTYPE and ENTITY declarations")
{
    systems::io::VtkXmlModelHandler handler;

    SECTION("DOCTYPE declaration")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_doctype.vtp";
        writeFile(input,
            "<?xml version=\"1.0\"?>\n"
            "<!DOCTYPE VTKFile SYSTEM \"http://evil.example.com/vtk.dtd\">\n"
                + std::string(kAsciiVtp));
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("ENTITY declaration")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_entity.vtp";
        writeFile(input,
            "<?xml version=\"1.0\"?>\n"
            "<!DOCTYPE VTKFile [<!ENTITY xxe SYSTEM \"file:///etc/passwd\">]>\n"
                + std::string(kAsciiVtp));
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("undefined entity reference in content")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_reft.vtp";
        std::string content = kAsciiVtp;
        const size_t pos = content.find("0 0 0 1 0 0");
        REQUIRE(pos != std::string::npos);
        content.replace(pos, 3, "&xx;");
        writeFile(input, content);
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }
}

TEST_CASE("VtkXmlModelHandler rejects corrupt files")
{
    systems::io::VtkXmlModelHandler handler;

    SECTION("missing Points section")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_nopts.vtp";
        writeFile(input,
            "<VTKFile type=\"PolyData\" version=\"0.1\">\n"
            "  <PolyData>\n"
            "    <Piece NumberOfPoints=\"0\" NumberOfPolys=\"0\"/>\n"
            "  </PolyData>\n"
            "</VTKFile>\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("cell references point out of range")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_oob.vtp";
        writeFile(input,
            "<VTKFile type=\"PolyData\" version=\"0.1\">\n"
            "  <PolyData>\n"
            "    <Piece NumberOfPoints=\"3\" NumberOfPolys=\"1\">\n"
            "      <Points><DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"ascii\">"
            "0 0 0 1 0 0 0 1 0</DataArray></Points>\n"
            "      <Polys>"
            "<DataArray type=\"Int64\" Name=\"connectivity\" format=\"ascii\">0 1 9</DataArray>"
            "<DataArray type=\"Int64\" Name=\"offsets\" format=\"ascii\">3</DataArray>"
            "</Polys>\n"
            "    </Piece>\n"
            "  </PolyData>\n"
            "</VTKFile>\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("unsupported dataset type")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_type.vtk";
        writeFile(input,
            "<VTKFile type=\"StructuredGrid\" version=\"0.1\">\n"
            "  <StructuredGrid/>\n"
            "</VTKFile>\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("missing file")
    {
        const fs::path input = core::TempFile::instance().path().string() + "_missing.vtp";
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("deeply nested tags overflow guard")
    {
        // 70 层嵌套超过解析深度上限：拒绝而非栈溢出崩溃
        std::string content;
        for (int i = 0; i < 70; ++i) {
            content += "<n>";
        }
        content += "<VTKFile type=\"PolyData\" version=\"0.1\"><PolyData/></VTKFile>";
        for (int i = 0; i < 70; ++i) {
            content += "</n>";
        }
        const fs::path input = core::TempFile::instance().path().string() + "_deep.vtp";
        writeFile(input, content);
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("inline binary prefix shorter than header size")
    {
        // 单独前缀编码分支：base64 解码结果不足 4 字节前缀，应报错而非越界读
        const fs::path input = core::TempFile::instance().path().string() + "_shortprefix.vtp";
        writeFile(input,
            "<VTKFile type=\"PolyData\" version=\"0.1\" byte_order=\"LittleEndian\">\n"
            "  <PolyData>\n"
            "    <Piece NumberOfPoints=\"3\" NumberOfPolys=\"0\">\n"
            "      <Points><DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"binary\">AAAA</DataArray></Points>\n"
            "    </Piece>\n"
            "  </PolyData>\n"
            "</VTKFile>\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }

    SECTION("raw appended offset near size_t max")
    {
        // offset 取 ULLONG_MAX 量级：减法形式的边界检查须拒绝而非加法回绕后越界读
        const fs::path input = core::TempFile::instance().path().string() + "_hugooffset.vtp";
        writeFile(input,
            "<VTKFile type=\"PolyData\" version=\"1.0\" byte_order=\"LittleEndian\" header_type=\"UInt64\">\n"
            "  <PolyData>\n"
            "    <Piece NumberOfPoints=\"3\" NumberOfPolys=\"1\">\n"
            "      <Points><DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"appended\" offset=\"18446744073709551615\"/></Points>\n"
            "      <Polys>"
            "<DataArray type=\"Int64\" Name=\"connectivity\" format=\"ascii\">0 1 2</DataArray>"
            "<DataArray type=\"Int64\" Name=\"offsets\" format=\"ascii\">3</DataArray>"
            "</Polys>\n"
            "    </Piece>\n"
            "  </PolyData>\n"
            "  <AppendedData encoding=\"raw\">_\n</AppendedData>\n"
            "</VTKFile>\n");
        REQUIRE_FALSE(handler.read_model(input, {}).has_value());
    }
}

TEST_CASE("VtkXmlModelHandler write_components round-trips vtu and vtp")
{
    systems::io::VtkXmlModelHandler handler;

    SECTION("vtu keeps edge/face/solid")
    {
        const fs::path out = core::TempFile::instance().path().string() + "_roundtrip.vtu";
        ModelLayer layer;
        const std::vector<Index> component_ids = addMeshComponent(layer, std::make_unique<MeshData>(makeMixedMesh()));

        REQUIRE_NOTHROW(handler.write_components(layer, component_ids, out, {}));
        REQUIRE(fs::exists(out));

        const ModelPayload payload = requireReadPayload(handler, out);
        const MeshData* mesh = payload.components.front()->mesh.get();
        REQUIRE(mesh->vertex_count_ == 4);
        REQUIRE(mesh->edge_vertices_ == std::vector<Index> { 0, 1 });
        REQUIRE(mesh->face_vertices_ == std::vector<Index> { 0, 1, 2 });
        REQUIRE(mesh->solid_types_ == std::vector<unsigned char> { 10 });
        REQUIRE(mesh->solid_vertices_ == std::vector<Index> { 0, 1, 2, 3 });
    }

    SECTION("vtp keeps face but drops solid")
    {
        const fs::path out = core::TempFile::instance().path().string() + "_roundtrip.vtp";
        ModelLayer layer;
        const std::vector<Index> component_ids = addMeshComponent(layer, std::make_unique<MeshData>(makeMixedMesh()));

        REQUIRE_NOTHROW(handler.write_components(layer, component_ids, out, {}));
        REQUIRE(fs::exists(out));

        const ModelPayload payload = requireReadPayload(handler, out);
        const MeshData* mesh = payload.components.front()->mesh.get();
        REQUIRE(mesh->face_vertices_ == std::vector<Index> { 0, 1, 2 });
        REQUIRE(mesh->edge_vertices_ == std::vector<Index> { 0, 1 });
        REQUIRE(mesh->solid_types_.empty());
    }
}

TEST_CASE("VtkXmlModelHandler write_components merges components with point offset")
{
    systems::io::VtkXmlModelHandler handler;
    const fs::path out = core::TempFile::instance().path().string() + "_merge.vtu";

    // 两个四面体组件：合并导出后 8 点 2 个四面体，第二个组件引用偏移后的点
    ModelLayer layer;
    const std::vector<Index> first_ids = addMeshComponent(layer, std::make_unique<MeshData>(makeMixedMesh()));
    const std::vector<Index> second_ids = addMeshComponent(layer, std::make_unique<MeshData>(makeMixedMesh()));

    std::vector<Index> component_ids = first_ids;
    component_ids.insert(component_ids.end(), second_ids.begin(), second_ids.end());

    REQUIRE_NOTHROW(handler.write_components(layer, component_ids, out, {}));
    REQUIRE(fs::exists(out));

    const ModelPayload payload = requireReadPayload(handler, out);
    const MeshData* mesh = payload.components.front()->mesh.get();
    REQUIRE(mesh->vertex_count_ == 8);
    REQUIRE(mesh->solid_types_.size() == 2);
    REQUIRE(mesh->solid_vertices_[4] >= 4);
    REQUIRE(mesh->solid_vertices_.back() < 8);
}
