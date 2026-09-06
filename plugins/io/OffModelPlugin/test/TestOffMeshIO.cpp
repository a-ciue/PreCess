/**
 * @file TestOffMeshIO.cpp
 * @brief OffMeshIO 的单元测试：ASCII 往返、属性变体、二进制读取与错误处理
 */
#include "MakeMeshData.h"
#include "OffMeshIO.h"
#include "TempFile.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {
//! @brief 取测试用临时文件路径（各用例复用同一路径，写前覆盖）
std::string tempOffPath(const std::string& tag)
{
    return core::TempFile::instance().path().string() + "." + tag + ".off";
}

//! @brief 以二进制方式写出文本内容，避免 Windows 下 '\n' 被改写
void writeTextFile(const std::string& path, const std::string& content)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.is_open());
    output << content;
    output.close();
}
} // namespace

// 测试名保持 ASCII：ctest 过滤器经控制台（GBK）传递会破坏中文名，导致用例匹配失败
TEST_CASE("OffMeshIO write/read round trip keeps points and faces")
{
    const MeshData source = MakeMeshData();
    const std::string path = tempOffPath("roundtrip");

    REQUIRE(OffMeshIO::write(path, source));

    MeshData loaded;
    REQUIRE(OffMeshIO::read(path, loaded));

    // OFF 只承载点与面：边与体单元不参与往返，坐标用 17 位有效数字写出，double 无损
    REQUIRE(loaded.vertex_positions_.size() == source.vertex_positions_.size());
    REQUIRE(loaded.vertex_positions_ == source.vertex_positions_);
    REQUIRE(loaded.face_vertices_ == source.face_vertices_);
    REQUIRE(loaded.face_vertices_offset_ == source.face_vertices_offset_);
    REQUIRE(loaded.vertex_count_ == static_cast<Index>(source.vertex_positions_.size()));
    REQUIRE(loaded.edge_vertices_.empty());
    REQUIRE(loaded.solid_vertices_.empty());
}

TEST_CASE("OffMeshIO reads ASCII variants with comments and attributes")
{
    const std::string path = tempOffPath("ascii_variant");
    MeshData mesh;

    SECTION("文件头注释与顶点法向附加分量（NOFF）")
    {
        const std::string content = "# 文件头注释\n"
                                    "\n"
                                    "NOFF\n"
                                    "4 4 0\n"
                                    "0 0 0 0 0 1\n"
                                    "1 0 0 1 0 0\n"
                                    "0 1 0 0 1 0\n"
                                    "0 0 1 0 0 1\n"
                                    "3 0 2 1\n"
                                    "3 0 1 3\n"
                                    "3 1 2 3\n"
                                    "3 0 3 2\n";
        writeTextFile(path, content);

        REQUIRE(OffMeshIO::read(path, mesh));
        REQUIRE(mesh.vertex_positions_.size() == 4);
        REQUIRE(mesh.vertex_count_ == 4);
        // 法向位于行尾被丢弃，坐标按前 3 个分量取用
        REQUIRE(mesh.vertex_positions_[1] == std::array<double, 3> { 1.0, 0.0, 0.0 });
        REQUIRE(mesh.vertex_positions_[3] == std::array<double, 3> { 0.0, 0.0, 1.0 });
        REQUIRE(mesh.face_vertices_.size() == 12);
        REQUIRE(mesh.face_vertices_offset_.size() == 5); // 4 个面
        REQUIRE(mesh.face_vertices_offset_.back() == 12);
    }

    SECTION("顶点与面颜色附加分量（COFF）")
    {
        const std::string content = "COFF\n"
                                    "4 1 0\n"
                                    "0 0 0 255 0 0\n"
                                    "1 0 0 0 255 0\n"
                                    "0 1 0 0 0 255\n"
                                    "0 0 1 128 128 128\n"
                                    "4 0 1 2 3 255 255 255\n";
        writeTextFile(path, content);

        REQUIRE(OffMeshIO::read(path, mesh));
        REQUIRE(mesh.vertex_positions_.size() == 4);
        REQUIRE(mesh.face_vertices_ == std::vector<Index> { 0, 1, 2, 3 }); // 四边形面原样保留，颜色被丢弃
        REQUIRE(mesh.face_vertices_offset_ == std::vector<Index> { 0, 4 });
    }

    SECTION("维度写在计数行上（nOFF）")
    {
        const std::string content = "nOFF\n"
                                    "3 4 2 0\n"
                                    "0 0 0\n"
                                    "1 0 0\n"
                                    "1 1 0\n"
                                    "0 1 0\n"
                                    "4 0 1 2 3\n"
                                    "3 0 1 2\n";
        writeTextFile(path, content);

        REQUIRE(OffMeshIO::read(path, mesh));
        REQUIRE(mesh.vertex_positions_.size() == 4);
        REQUIRE(mesh.face_vertices_.size() == 7);
        REQUIRE(mesh.face_vertices_offset_ == std::vector<Index> { 0, 4, 7 });
    }
}

TEST_CASE("OffMeshIO reads binary OFF")
{
    // 手工拼装 Geomview 二进制 OFF：小端 int32 计数/索引 + float32 坐标
    std::vector<char> bytes;
    auto append_int32 = [&bytes](int32_t value) {
        const auto raw = static_cast<uint32_t>(value);
        for (int i = 0; i < 4; ++i) {
            bytes.push_back(static_cast<char>((raw >> (8 * i)) & 0xFFu));
        }
    };
    auto append_float32 = [&append_int32](float value) {
        uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        append_int32(static_cast<int32_t>(bits));
    };

    const char* header = "OFF BINARY\n";
    bytes.insert(bytes.end(), header, header + std::strlen(header));

    append_int32(4); // 顶点数
    append_int32(2); // 面数
    append_int32(0); // 边数

    const std::array<std::array<float, 3>, 4> points { {
        { 0.0F, 0.0F, 0.0F },
        { 1.0F, 0.0F, 0.0F },
        { 0.0F, 1.0F, 0.0F },
        { 0.0F, 0.0F, 1.0F },
    } };
    for (const auto& point : points) {
        for (float coordinate : point) {
            append_float32(coordinate);
        }
    }
    append_int32(3);
    append_int32(0);
    append_int32(2);
    append_int32(1);
    append_int32(3);
    append_int32(0);
    append_int32(1);
    append_int32(3);

    const std::string path = tempOffPath("binary");
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    REQUIRE(output.is_open());
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    output.close();

    MeshData mesh;
    REQUIRE(OffMeshIO::read(path, mesh));
    REQUIRE(mesh.vertex_positions_.size() == 4);
    REQUIRE(mesh.vertex_count_ == 4);
    REQUIRE(mesh.vertex_positions_[3] == std::array<double, 3> { 0.0, 0.0, 1.0 });
    REQUIRE(mesh.face_vertices_ == std::vector<Index> { 0, 2, 1, 0, 1, 3 });
    REQUIRE(mesh.face_vertices_offset_.size() == 3);
}

TEST_CASE("OffMeshIO rejects corrupt or invalid files")
{
    const std::string path = tempOffPath("invalid");
    MeshData mesh;

    SECTION("空文件")
    {
        writeTextFile(path, "");
        REQUIRE_FALSE(OffMeshIO::read(path, mesh));
    }

    SECTION("头部不是 OFF 关键字")
    {
        writeTextFile(path, "STL\n0 0 0\n");
        REQUIRE_FALSE(OffMeshIO::read(path, mesh));
    }

    SECTION("面点索引越界")
    {
        const std::string content = "OFF\n"
                                    "2 1 0\n"
                                    "0 0 0\n"
                                    "1 0 0\n"
                                    "3 0 1 5\n";
        writeTextFile(path, content);
        REQUIRE_FALSE(OffMeshIO::read(path, mesh));
    }

    SECTION("顶点记录被截断")
    {
        const std::string content = "OFF\n"
                                    "3 1 0\n"
                                    "0 0 0\n"
                                    "1 0 0\n";
        writeTextFile(path, content);
        REQUIRE_FALSE(OffMeshIO::read(path, mesh));
    }

    SECTION("二进制计数超出文件长度")
    {
        const std::string content = "OFF BINARY\n";
        writeTextFile(path, content);
        REQUIRE_FALSE(OffMeshIO::read(path, mesh));
    }
}

TEST_CASE("OffMeshIO write skips degenerate and dirty faces")
{
    MeshData mesh;
    mesh.init();
    mesh.vertex_positions_ = {
        { 0.0, 0.0, 0.0 },
        { 1.0, 0.0, 0.0 },
        { 0.0, 1.0, 0.0 },
    };
    // 面0：合法三角形；面1：两点退化面；面2：点索引越界
    mesh.face_vertices_ = { 0, 1, 2, 1, 2, 0, 1, 7 };
    mesh.face_vertices_offset_ = { 0, 3, 5, 8 };
    mesh.vertex_count_ = 3;

    const std::string path = tempOffPath("skip_faces");
    REQUIRE(OffMeshIO::write(path, mesh));

    MeshData loaded;
    REQUIRE(OffMeshIO::read(path, loaded));
    REQUIRE(loaded.vertex_positions_.size() == 3);
    REQUIRE(loaded.face_vertices_ == std::vector<Index> { 0, 1, 2 });
    REQUIRE(loaded.face_vertices_offset_ == std::vector<Index> { 0, 3 });
}
