/**
 * @file VtkXmlModelHandler.cpp
 * @brief VTK XML 网格文件读写实现
 *
 * 覆盖两种 VTK XML 数据集：
 * - .vtp（PolyData）：Points + Lines（折线段 -> 边单元）+ Polys（-> 面单元），
 *   Verts / Strips 告警忽略；
 * - .vtu（UnstructuredGrid）：Points + Cells（connectivity / offsets / types），
 *   按单元 VTK 类型码分发到边（线/折线）、面（三角形/四边形/多边形）、
 *   体（四面体/六面体/三棱柱/金字塔）；多面体与高阶单元告警跳过。
 *
 * 数据承载支持 ascii、base64 二进制（inline 与 appended 两种布局）与
 * encoding="raw" 的 appended 原始二进制（VTK 9 / ParaView 新版默认，详见
 * VtkXmlReader.cpp）；压缩数据（compress 属性）显式报不支持。纯点数据集
 * （Verts-only 点云）读入为只有顶点的网格组件。写出统一为 ascii inline；
 * 扩展名 .vtp 写 PolyData（仅面/边，体单元告警丢弃），.vtu 写
 * UnstructuredGrid（边/面/体全量）。
 */
#include "VtkXmlModelHandler.h"

#include "ArgType.h"
#include "ComponentData.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelLayer.h"
#include "VtkXmlReader.h"

#include <spdlog/spdlog.h>

#include <array>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

//! @brief 读入侧用到的 VTK 单元类型码
constexpr int64_t kVtkLine = 3;
constexpr int64_t kVtkPolyLine = 4;
constexpr int64_t kVtkTriangle = 5;
constexpr int64_t kVtkPolygon = 7;
constexpr int64_t kVtkQuad = 9;
constexpr int64_t kVtkTetra = 10;
constexpr int64_t kVtkHexahedron = 12;
constexpr int64_t kVtkWedge = 13;
constexpr int64_t kVtkPyramid = 14;
constexpr int64_t kVtkPolyhedron = 42;

//! @brief VTK 简单体单元的角点数（读入校验与写出口径一致）
size_t solidCornerCount(unsigned char vtk_type)
{
    switch (vtk_type) {
    case kVtkTetra:
        return 4;
    case kVtkHexahedron:
        return 8;
    case kVtkWedge:
        return 6;
    case kVtkPyramid:
        return 5;
    default:
        return 0; // 多面体 / 高阶等需要面拓扑或不受支持的类型
    }
}

//! @brief 读 Piece 的 Points 段：3 分量坐标流展开为顶点数组
bool readPoints(const vtkxml::XmlNode& piece, const vtkxml::XmlDocument& doc, MeshData& mesh)
{
    const vtkxml::XmlNode* points = piece.child("Points");
    const vtkxml::XmlNode* array = points ? points->child("DataArray") : nullptr;
    if (!array) {
        spdlog::error("VtkXmlModelHandler: Piece has no Points/DataArray");
        return false;
    }
    if (const std::string* components = array->attribute("NumberOfComponents");
        components && *components != "3") {
        spdlog::error("VtkXmlModelHandler: Points NumberOfComponents '{}' is not 3", *components);
        return false;
    }

    const std::vector<double> coordinates = vtkxml::readDoubles(*array, doc);
    if (coordinates.empty() || coordinates.size() % 3 != 0) {
        spdlog::error("VtkXmlModelHandler: Points coordinate stream is empty or not a multiple of 3");
        return false;
    }
    for (size_t v = 0; v + 2 < coordinates.size(); v += 3) {
        mesh.vertex_positions_.push_back({ coordinates[v], coordinates[v + 1], coordinates[v + 2] });
    }
    mesh.vertex_count_ = static_cast<Index>(mesh.vertex_positions_.size());
    return true;
}

//! @brief 装配目标：把文件点 id 段换算为局部点 id 后分发到三类单元数组
class CellAssembler {
public:
    explicit CellAssembler(MeshData& mesh)
        : mesh_(mesh)
    {
    }

    //! @brief 装配一个面单元（文件点 id 段），越界引用返回 false
    bool appendFace(const std::vector<int64_t>& connectivity, size_t begin, size_t end)
    {
        if (end - begin < 3) {
            spdlog::warn("VtkXmlModelHandler: degenerate face with {} corners skipped", end - begin);
            return true;
        }
        std::vector<Index> corners;
        if (!mapCorners(connectivity, begin, end, corners)) {
            return false;
        }
        mesh_.face_vertices_.insert(mesh_.face_vertices_.end(), corners.begin(), corners.end());
        mesh_.face_vertices_offset_.push_back(static_cast<Index>(mesh_.face_vertices_.size()));
        ++cell_count_;
        return true;
    }

    //! @brief 装配一段折线（文件点 id 段）为相邻点对的边单元
    bool appendPolyline(const std::vector<int64_t>& connectivity, size_t begin, size_t end)
    {
        if (end - begin < 2) {
            return true; // 单点段不构成边，静默忽略
        }
        std::vector<Index> corners;
        if (!mapCorners(connectivity, begin, end, corners)) {
            return false;
        }
        for (size_t c = 0; c + 1 < corners.size(); ++c) {
            mesh_.edge_vertices_.push_back(corners[c]);
            mesh_.edge_vertices_.push_back(corners[c + 1]);
            ++cell_count_;
        }
        return true;
    }

    //! @brief 装配一个简单体单元（文件点 id 段 + VTK 类型码）
    bool appendSolid(const std::vector<int64_t>& connectivity, size_t begin, size_t end,
        unsigned char vtk_type)
    {
        const size_t expected = solidCornerCount(vtk_type);
        if (expected == 0 || end - begin != expected) {
            spdlog::warn("VtkXmlModelHandler: solid type {} with {} corners skipped",
                static_cast<int>(vtk_type), end - begin);
            return true;
        }
        std::vector<Index> corners;
        if (!mapCorners(connectivity, begin, end, corners)) {
            return false;
        }
        mesh_.solid_types_.push_back(vtk_type);
        mesh_.solid_vertices_.insert(mesh_.solid_vertices_.end(), corners.begin(), corners.end());
        mesh_.solid_vertices_offset_.push_back(static_cast<Index>(mesh_.solid_vertices_.size()));
        mesh_.solid_faces_offset_.push_back(0);
        ++cell_count_;
        return true;
    }

    //! @brief 未知/不支持类型的单元计数（汇总告警用）
    void skipCell(int64_t vtk_type) { ++skipped_types_[vtk_type]; }

    size_t cellCount() const { return cell_count_; }

    void reportSkipped() const
    {
        for (const auto& [type, count] : skipped_types_) {
            spdlog::warn("VtkXmlModelHandler: {} cell(s) of VTK type {} skipped", count, type);
        }
    }

private:
    //! @brief 文件点 id -> 局部点 id，越界返回 false 并记录错误
    bool mapCorners(const std::vector<int64_t>& connectivity, size_t begin, size_t end,
        std::vector<Index>& corners) const
    {
        corners.reserve(end - begin);
        for (size_t i = begin; i < end; ++i) {
            if (connectivity[i] < 0 || connectivity[i] >= mesh_.vertex_count_) {
                spdlog::error("VtkXmlModelHandler: cell references point {} out of {}",
                    connectivity[i], mesh_.vertex_count_);
                return false;
            }
            corners.push_back(static_cast<Index>(connectivity[i]));
        }
        return true;
    }

    MeshData& mesh_;
    size_t cell_count_ { 0 };
    std::map<int64_t, size_t> skipped_types_;
};

//! @brief 读 connectivity / offsets 形式的段列表（Lines 与 Polys 共用）
bool readSegments(const vtkxml::XmlNode& parent, const vtkxml::XmlDocument& doc,
    std::vector<int64_t>& connectivity, std::vector<int64_t>& offsets)
{
    const vtkxml::XmlNode* conn_node = nullptr;
    const vtkxml::XmlNode* offset_node = nullptr;
    for (const auto& node : parent.children) {
        if (node.name != "DataArray") {
            continue;
        }
        const std::string* name = node.attribute("Name");
        if (!name) {
            continue;
        }
        if (*name == "connectivity") {
            conn_node = &node;
        } else if (*name == "offsets") {
            offset_node = &node;
        }
    }
    if (!conn_node || !offset_node) {
        spdlog::error("VtkXmlModelHandler: '{}' misses connectivity/offsets DataArray",
            parent.name);
        return false;
    }
    connectivity = vtkxml::readIntegers(*conn_node, doc);
    offsets = vtkxml::readIntegers(*offset_node, doc);
    return true;
}

//! @brief 逐段回调装配：offsets 为段尾位置（1 基累计），段 i 为 [offsets[i-1], offsets[i])
template <typename Append>
bool forEachSegment(const std::vector<int64_t>& connectivity, const std::vector<int64_t>& offsets,
    Append&& append)
{
    int64_t previous = 0;
    for (const int64_t offset : offsets) {
        if (offset < previous || static_cast<size_t>(offset) > connectivity.size()) {
            spdlog::error("VtkXmlModelHandler: offsets not monotonic or exceed connectivity");
            return false;
        }
        if (!append(connectivity, static_cast<size_t>(previous), static_cast<size_t>(offset))) {
            return false;
        }
        previous = offset;
    }
    if (previous != static_cast<int64_t>(connectivity.size()) && !connectivity.empty()) {
        spdlog::error("VtkXmlModelHandler: last offset does not cover connectivity");
        return false;
    }
    return true;
}

//! @brief 读 PolyData：Lines -> 边、Polys -> 面；Verts/Strips 告警忽略
bool readPolyData(const vtkxml::XmlNode& root, const vtkxml::XmlDocument& doc, MeshData& mesh)
{
    const vtkxml::XmlNode* poly_data = root.child("PolyData");
    if (!poly_data) {
        spdlog::error("VtkXmlModelHandler: VTKFile has no PolyData element");
        return false;
    }

    CellAssembler assembler(mesh);
    bool piece_seen = false;
    for (const auto& piece : poly_data->children) {
        if (piece.name != "Piece") {
            continue;
        }
        // vtp 的多 Piece 数据集各持有局部 0 基点 id（.pvtp 拆分文件场景），v1 只取第一块
        if (piece_seen) {
            spdlog::warn("VtkXmlModelHandler: multi-piece dataset, extra Piece skipped");
            break;
        }
        piece_seen = true;
        if (!readPoints(piece, doc, mesh)) {
            return false;
        }
        if (const vtkxml::XmlNode* lines = piece.child("Lines")) {
            std::vector<int64_t> connectivity, offsets;
            if (!readSegments(*lines, doc, connectivity, offsets)) {
                return false;
            }
            if (!forEachSegment(connectivity, offsets,
                    [&](const std::vector<int64_t>& c, size_t b, size_t e) {
                        return assembler.appendPolyline(c, b, e);
                    })) {
                return false;
            }
        }
        if (const vtkxml::XmlNode* polys = piece.child("Polys")) {
            std::vector<int64_t> connectivity, offsets;
            if (!readSegments(*polys, doc, connectivity, offsets)) {
                return false;
            }
            if (!forEachSegment(connectivity, offsets,
                    [&](const std::vector<int64_t>& c, size_t b, size_t e) {
                        return assembler.appendFace(c, b, e);
                    })) {
                return false;
            }
        }
        if (piece.child("Verts")) {
            spdlog::warn("VtkXmlModelHandler: PolyData Verts are not mesh cells, ignored");
        }
        if (piece.child("Strips")) {
            spdlog::warn("VtkXmlModelHandler: PolyData Strips are not supported, ignored");
        }
    }
    assembler.reportSkipped();
    // 纯点文件（Verts-only 点云）合法：顶点即数据，无单元也接受
    return mesh.vertex_count_ > 0;
}

//! @brief 读 UnstructuredGrid：Cells 按 VTK 类型码分发到边/面/体
bool readUnstructuredGrid(const vtkxml::XmlNode& root, const vtkxml::XmlDocument& doc, MeshData& mesh)
{
    const vtkxml::XmlNode* grid = root.child("UnstructuredGrid");
    if (!grid) {
        spdlog::error("VtkXmlModelHandler: VTKFile has no UnstructuredGrid element");
        return false;
    }

    CellAssembler assembler(mesh);
    bool piece_seen = false;
    for (const auto& piece : grid->children) {
        if (piece.name != "Piece") {
            continue;
        }
        // vtu 的多 Piece 数据集各持有局部 0 基点 id（.pvtu 拆分文件场景），v1 只取第一块
        if (piece_seen) {
            spdlog::warn("VtkXmlModelHandler: multi-piece dataset, extra Piece skipped");
            break;
        }
        piece_seen = true;
        if (!readPoints(piece, doc, mesh)) {
            return false;
        }
        const vtkxml::XmlNode* cells = piece.child("Cells");
        if (!cells) {
            spdlog::error("VtkXmlModelHandler: Piece has no Cells element");
            return false;
        }
        std::vector<int64_t> connectivity, offsets;
        if (!readSegments(*cells, doc, connectivity, offsets)) {
            return false;
        }
        const vtkxml::XmlNode* types_array = nullptr;
        for (const auto& node : cells->children) {
            if (node.name == "DataArray" && node.attribute("Name")
                && *node.attribute("Name") == "types") {
                types_array = &node;
                break;
            }
        }
        if (!types_array) {
            spdlog::error("VtkXmlModelHandler: Cells misses 'types' DataArray");
            return false;
        }
        const std::vector<int64_t> types = vtkxml::readIntegers(*types_array, doc);
        if (types.size() != offsets.size()) {
            spdlog::error("VtkXmlModelHandler: Cells types count {} != offsets count {}",
                types.size(), offsets.size());
            return false;
        }

        // 逐单元按类型分发；引用越界（append 返回 false）整体读取失败
        size_t cell_index = 0;
        if (!forEachSegment(connectivity, offsets,
                [&](const std::vector<int64_t>& c, size_t b, size_t e) {
                    const int64_t vtk_type = types[cell_index++];
                    switch (vtk_type) {
                    case kVtkLine:
                    case kVtkPolyLine:
                        return assembler.appendPolyline(c, b, e);
                    case kVtkTriangle:
                    case kVtkQuad:
                    case kVtkPolygon:
                        return assembler.appendFace(c, b, e);
                    case kVtkTetra:
                    case kVtkHexahedron:
                    case kVtkWedge:
                    case kVtkPyramid:
                        return assembler.appendSolid(c, b, e, static_cast<unsigned char>(vtk_type));
                    default:
                        assembler.skipCell(vtk_type);
                        return true;
                    }
                })) {
            return false;
        }
    }
    assembler.reportSkipped();
    // 纯点数据集（无 Cells 单元）与 vtp 同口径：顶点即数据
    return mesh.vertex_count_ > 0;
}

/**
 * @brief 把一个组件的网格追加到 merged，多组件导出时按点偏移拼成一个网格
 *
 * MeshData 自包含、连通性存组件内局部点索引，追加时统一加 vertex_offset。
 * 多面体（VTK_POLYHEDRON，需要面拓扑数组）与未知体类型告警跳过；
 * 面/边单元与简单体单元全量平移。
 * @param vertex_offset 入参为当前文件内点偏移，出参累加本组件的点数
 */
bool appendComponentMesh(const ComponentData& component, MeshData& merged, Index& vertex_offset)
{
    const MeshData* source = component.mesh.get();
    if (!source) {
        return false;
    }

    const Index point_count = static_cast<Index>(source->vertex_positions_.size());
    if (point_count <= 0) {
        spdlog::warn("VtkXmlModelHandler: component {} has no vertices, skip", component.id);
        return false;
    }

    merged.vertex_positions_.insert(merged.vertex_positions_.end(),
        source->vertex_positions_.begin(), source->vertex_positions_.end());

    // 边单元：每 2 点一段；先局部收集校验，出现脏边则整组丢弃
    if (source->edge_vertices_.size() % 2 == 0) {
        std::vector<Index> local_edges;
        local_edges.reserve(source->edge_vertices_.size());
        bool edges_ok = true;
        for (const Index point_id : source->edge_vertices_) {
            if (point_id < 0 || point_id >= point_count) {
                edges_ok = false;
                break;
            }
            local_edges.push_back(vertex_offset + point_id);
        }
        if (edges_ok) {
            merged.edge_vertices_.insert(merged.edge_vertices_.end(),
                local_edges.begin(), local_edges.end());
        } else {
            spdlog::warn("VtkXmlModelHandler: component {} has dirty edge, all edges skipped",
                component.id);
        }
    }

    Index skipped_faces = 0;
    if (source->face_vertices_offset_.size() >= 2) {
        const Index face_count = static_cast<Index>(source->face_vertices_offset_.size() - 1);
        const Index corner_count = static_cast<Index>(source->face_vertices_.size());
        for (Index f = 0; f < face_count; ++f) {
            const Index begin = source->face_vertices_offset_[static_cast<size_t>(f)];
            const Index end = source->face_vertices_offset_[static_cast<size_t>(f) + 1];
            if (begin < 0 || end < begin || end > corner_count || end - begin < 3) {
                ++skipped_faces;
                continue;
            }
            bool face_ok = true;
            std::vector<Index> corners(static_cast<size_t>(end - begin));
            for (Index c = begin; c < end; ++c) {
                const Index point_id = source->face_vertices_[static_cast<size_t>(c)];
                if (point_id < 0 || point_id >= point_count) {
                    face_ok = false;
                    break;
                }
                corners[static_cast<size_t>(c - begin)] = vertex_offset + point_id;
            }
            if (!face_ok) {
                ++skipped_faces;
                continue;
            }
            merged.face_vertices_.insert(merged.face_vertices_.end(), corners.begin(), corners.end());
            merged.face_vertices_offset_.push_back(static_cast<Index>(merged.face_vertices_.size()));
        }
    }
    if (skipped_faces > 0) {
        spdlog::warn("VtkXmlModelHandler: component {} has {} dirty/degenerate face(s), skip",
            component.id, skipped_faces);
    }

    Index skipped_solids = 0;
    if (source->solid_vertices_offset_.size() >= 2
        && source->solid_types_.size() + 1 == source->solid_vertices_offset_.size()) {
        const Index solid_count = static_cast<Index>(source->solid_types_.size());
        const Index corner_count = static_cast<Index>(source->solid_vertices_.size());
        for (Index s = 0; s < solid_count; ++s) {
            const unsigned char vtk_type = source->solid_types_[static_cast<size_t>(s)];
            const Index begin = source->solid_vertices_offset_[static_cast<size_t>(s)];
            const Index end = source->solid_vertices_offset_[static_cast<size_t>(s) + 1];
            if (solidCornerCount(vtk_type) == 0
                || begin < 0 || end > corner_count
                || end - begin != static_cast<Index>(solidCornerCount(vtk_type))) {
                ++skipped_solids;
                continue;
            }
            bool solid_ok = true;
            std::vector<Index> corners(static_cast<size_t>(end - begin));
            for (Index c = begin; c < end; ++c) {
                const Index point_id = source->solid_vertices_[static_cast<size_t>(c)];
                if (point_id < 0 || point_id >= point_count) {
                    solid_ok = false;
                    break;
                }
                corners[static_cast<size_t>(c - begin)] = vertex_offset + point_id;
            }
            if (!solid_ok) {
                ++skipped_solids;
                continue;
            }
            merged.solid_types_.push_back(vtk_type);
            merged.solid_vertices_.insert(merged.solid_vertices_.end(), corners.begin(), corners.end());
            merged.solid_vertices_offset_.push_back(static_cast<Index>(merged.solid_vertices_.size()));
            merged.solid_faces_offset_.push_back(0);
        }
    }
    if (skipped_solids > 0) {
        spdlog::warn("VtkXmlModelHandler: component {} has {} unsupported solid(s), skip",
            component.id, skipped_solids);
    }

    vertex_offset += point_count;
    return true;
}

//! @brief 面单元写出用的 VTK 类型码：三角形/四边形精确、其余按多边形
int64_t faceVtkType(size_t corner_count)
{
    if (corner_count == 3) {
        return kVtkTriangle;
    }
    return corner_count == 4 ? kVtkQuad : kVtkPolygon;
}

//! @brief 把数值序列写成空格分隔的单行文本（ascii DataArray 载荷）
template <typename T>
void writeAsciiArray(std::ostream& output, const std::vector<T>& values)
{
    for (size_t i = 0; i < values.size(); ++i) {
        output << (i == 0 ? "" : " ") << values[i];
    }
}

//! @brief 写出 .vtp（PolyData）：Lines 承载边、Polys 承载面；体单元告警丢弃
bool writeVtp(const std::filesystem::path& path, const MeshData& mesh)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        spdlog::error("VtkXmlModelHandler: failed to open file '{}' for writing", path.string());
        return false;
    }
    output << std::setprecision(17);

    const Index edge_count = static_cast<Index>(mesh.edge_vertices_.size() / 2);
    const Index face_count = mesh.face_vertices_offset_.size() >= 2
        ? static_cast<Index>(mesh.face_vertices_offset_.size() - 1)
        : 0;
    if (!mesh.solid_types_.empty()) {
        spdlog::warn("VtkXmlModelHandler: {} solid cell(s) cannot be stored in PolyData, dropped",
            mesh.solid_types_.size());
    }

    output << "<?xml version=\"1.0\"?>\n";
    output << "<VTKFile type=\"PolyData\" version=\"0.1\" byte_order=\"LittleEndian\">\n";
    output << "  <PolyData>\n";
    output << "    <Piece NumberOfPoints=\"" << mesh.vertex_count_
           << "\" NumberOfVerts=\"0\" NumberOfLines=\"" << edge_count
           << "\" NumberOfStrips=\"0\" NumberOfPolys=\"" << face_count << "\">\n";

    output << "      <Points>\n";
    output << "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"ascii\">\n";
    for (const auto& position : mesh.vertex_positions_) {
        output << "          " << position[0] << ' ' << position[1] << ' ' << position[2] << '\n';
    }
    output << "        </DataArray>\n";
    output << "      </Points>\n";

    output << "      <Lines>\n";
    output << "        <DataArray type=\"Int64\" Name=\"connectivity\" format=\"ascii\">";
    writeAsciiArray(output, mesh.edge_vertices_);
    output << "</DataArray>\n";
    output << "        <DataArray type=\"Int64\" Name=\"offsets\" format=\"ascii\">";
    for (Index e = 0; e < edge_count; ++e) {
        output << (e == 0 ? "" : " ") << (e + 1) * 2;
    }
    output << "</DataArray>\n";
    output << "      </Lines>\n";

    output << "      <Polys>\n";
    output << "        <DataArray type=\"Int64\" Name=\"connectivity\" format=\"ascii\">";
    writeAsciiArray(output, mesh.face_vertices_);
    output << "</DataArray>\n";
    output << "        <DataArray type=\"Int64\" Name=\"offsets\" format=\"ascii\">";
    for (Index f = 0; f < face_count; ++f) {
        output << (f == 0 ? "" : " ") << mesh.face_vertices_offset_[static_cast<size_t>(f) + 1];
    }
    output << "</DataArray>\n";
    output << "      </Polys>\n";

    output << "    </Piece>\n";
    output << "  </PolyData>\n";
    output << "</VTKFile>\n";

    output.flush();
    return static_cast<bool>(output);
}

//! @brief 写出 .vtu（UnstructuredGrid）：边/面/体全量，单元按 VTK 类型码标注
bool writeVtu(const std::filesystem::path& path, const MeshData& mesh)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        spdlog::error("VtkXmlModelHandler: failed to open file '{}' for writing", path.string());
        return false;
    }
    output << std::setprecision(17);

    // 单元序列：边（2 点线段）-> 面 -> 体；connectivity/offsets/types 同序生成
    std::vector<Index> connectivity;
    std::vector<Index> offsets;
    std::vector<int64_t> types;
    for (size_t e = 0; e + 1 < mesh.edge_vertices_.size(); e += 2) {
        connectivity.push_back(mesh.edge_vertices_[e]);
        connectivity.push_back(mesh.edge_vertices_[e + 1]);
        offsets.push_back(static_cast<Index>(connectivity.size()));
        types.push_back(kVtkLine);
    }
    if (mesh.face_vertices_offset_.size() >= 2) {
        for (size_t f = 0; f + 1 < mesh.face_vertices_offset_.size(); ++f) {
            const Index begin = mesh.face_vertices_offset_[f];
            const Index end = mesh.face_vertices_offset_[f + 1];
            for (Index c = begin; c < end; ++c) {
                connectivity.push_back(mesh.face_vertices_[static_cast<size_t>(c)]);
            }
            offsets.push_back(static_cast<Index>(connectivity.size()));
            types.push_back(faceVtkType(static_cast<size_t>(end - begin)));
        }
    }
    for (size_t s = 0; s < mesh.solid_types_.size(); ++s) {
        const Index begin = mesh.solid_vertices_offset_[s];
        const Index end = mesh.solid_vertices_offset_[s + 1];
        for (Index c = begin; c < end; ++c) {
            connectivity.push_back(mesh.solid_vertices_[static_cast<size_t>(c)]);
        }
        offsets.push_back(static_cast<Index>(connectivity.size()));
        types.push_back(mesh.solid_types_[s]);
    }

    output << "<?xml version=\"1.0\"?>\n";
    output << "<VTKFile type=\"UnstructuredGrid\" version=\"0.1\" byte_order=\"LittleEndian\">\n";
    output << "  <UnstructuredGrid>\n";
    output << "    <Piece NumberOfPoints=\"" << mesh.vertex_count_
           << "\" NumberOfCells=\"" << types.size() << "\">\n";
    output << "      <Points>\n";
    output << "        <DataArray type=\"Float64\" NumberOfComponents=\"3\" format=\"ascii\">\n";
    for (const auto& position : mesh.vertex_positions_) {
        output << "          " << position[0] << ' ' << position[1] << ' ' << position[2] << '\n';
    }
    output << "        </DataArray>\n";
    output << "      </Points>\n";
    output << "      <Cells>\n";
    output << "        <DataArray type=\"Int64\" Name=\"connectivity\" format=\"ascii\">";
    writeAsciiArray(output, connectivity);
    output << "</DataArray>\n";
    output << "        <DataArray type=\"Int64\" Name=\"offsets\" format=\"ascii\">";
    writeAsciiArray(output, offsets);
    output << "</DataArray>\n";
    output << "        <DataArray type=\"UInt8\" Name=\"types\" format=\"ascii\">";
    writeAsciiArray(output, types);
    output << "</DataArray>\n";
    output << "      </Cells>\n";
    output << "    </Piece>\n";
    output << "  </UnstructuredGrid>\n";
    output << "</VTKFile>\n";

    output.flush();
    return static_cast<bool>(output);
}

} // namespace

namespace systems::io {
using core::ArgType;

std::optional<ModelPayload> VtkXmlModelHandler::read_model(const fs::path& path, const std::vector<std::any>& args)
{
    // vtp/vtu 承载点与边/面/体单元，读入结果是一个网格组件
    auto mesh = std::make_unique<MeshData>();
    mesh->init();

    try {
        const vtkxml::XmlDocument doc = vtkxml::XmlDocument::load(path);
        const std::string dataset = doc.root().attribute("type") ? *doc.root().attribute("type") : "";
        bool ok = false;
        if (dataset == "PolyData") {
            ok = readPolyData(doc.root(), doc, *mesh);
        } else if (dataset == "UnstructuredGrid") {
            ok = readUnstructuredGrid(doc.root(), doc, *mesh);
        } else {
            spdlog::error("VtkXmlModelHandler: unsupported VTKFile type '{}'", dataset);
            ok = false;
        }
        if (!ok) {
            spdlog::error("VtkXmlModelHandler: failed to read VTK XML file: {}", path.string());
            return std::nullopt;
        }
    } catch (const vtkxml::ReadError& e) {
        spdlog::error("VtkXmlModelHandler: {}: {}", e.what(), path.string());
        return std::nullopt;
    }

    auto component = std::make_unique<ComponentData>();
    component->id = -1; // 组件 id 由模型层入池时分配
    component->name = "Comp_0"; // vtp/vtu 无分组概念，整个文件作为一个组件
    component->mesh = std::move(mesh);

    ComponentDatas components;
    components.push_back(std::move(component));

    return ModelPayload { path.filename().u8string(), std::move(components) };
}

void VtkXmlModelHandler::write_components(const ModelLayer& mgr,
    const std::vector<Index>& component_ids,
    const fs::path& path,
    const std::vector<std::any>& /*args*/)
{
    if (component_ids.empty()) {
        spdlog::error("VtkXmlModelHandler: write_components called with empty component_ids");
        return;
    }

    MeshData merged;
    merged.init();

    Index vertex_offset = 0;
    int merged_count = 0;
    for (Index cid : component_ids) {
        const ComponentData* component = mgr.findComponent(cid);
        if (!component) {
            spdlog::warn("VtkXmlModelHandler: component {} not found, skip", cid);
            continue;
        }
        if (!component->mesh) {
            spdlog::warn("VtkXmlModelHandler: component {} has no mesh, skip", cid);
            continue;
        }

        if (appendComponentMesh(*component, merged, vertex_offset)) {
            ++merged_count;
        }
    }

    const bool has_cells = !merged.edge_vertices_.empty()
        || merged.face_vertices_offset_.size() >= 2
        || !merged.solid_types_.empty();
    if (merged_count == 0 || !has_cells) {
        spdlog::error("VtkXmlModelHandler: no mesh component to export");
        return;
    }

    merged.vertex_count_ = static_cast<Index>(merged.vertex_positions_.size());
    const bool want_poly_data = path.extension() == ".vtp";
    const bool written = want_poly_data ? writeVtp(path, merged) : writeVtu(path, merged);
    if (written) {
        spdlog::info("VtkXmlModelHandler: wrote {} file: {} (components_merged={})",
            want_poly_data ? "vtp" : "vtu", path.string(), merged_count);
    } else {
        spdlog::error("VtkXmlModelHandler: failed to write VTK XML file: {}", path.string());
    }
}

std::vector<ArgType> VtkXmlModelHandler::read_args_type() const
{
    return {};
}

std::vector<ArgType> VtkXmlModelHandler::write_args_type() const
{
    return {};
}
}
