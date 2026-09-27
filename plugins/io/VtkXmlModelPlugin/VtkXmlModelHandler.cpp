/**
 * @file VtkXmlModelHandler.cpp
 * @brief VTK XML 网格文件读写实现（基于 VTK IOXML 读写器）
 *
 * 读取与写出分别经 vtkXMLPolyDataReader / vtkXMLUnstructuredGridReader 与
 * vtkXMLPolyDataWriter / vtkXMLUnstructuredGridWriter 完成，天然覆盖 VTK
 * XML 的全部数据承载（ascii、base64 二进制 inline/appended、encoding="raw"
 * 的原始二进制）与压缩数据（compressor，zlib/lz4/lzma）。
 *
 * 安全约定（不可信 XML）：交给 VTK（expat）解析之前先做字节级预检，
 * 命中 DOCTYPE / ENTITY 声明直接拒绝，不从外部加载任何资源。
 *
 * 单元映射：Lines 折线 -> 边单元（相邻点对）、Polys 与二维 cell -> 面单元、
 * 三维 cell（四面体/六面体/三棱柱/金字塔等）-> 体单元（沿用 VTK 类型码）。
 * 写出统一 ascii；扩展名 .vtp 写 PolyData（体单元告警丢弃），.vtu 写
 * UnstructuredGrid（边/面/体全量）。非 ASCII 路径经临时文件中转。
 */
#include "VtkXmlModelHandler.h"

#include "ArgType.h"
#include "ComponentData.h"
#include "MeshData.h"
#include "ModelData.h"
#include "ModelLayer.h"
#include "TempFile.h"

#include <spdlog/spdlog.h>
#include <vtkCell.h>
#include <vtkCellArray.h>
#include <vtkCellType.h>
#include <vtkIdList.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkXMLUnstructuredGridReader.h>
#include <vtkXMLUnstructuredGridWriter.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

//! @brief 读文件全部字节，失败返回空（预检与类型探测共用）
std::string readAllBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }
    return std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

//! @brief 路径是否全 ASCII（vtkXMLReader 的窄字符文件名在 Windows 下仅支持 ASCII）
bool isAsciiPath(const std::filesystem::path& path)
{
    const auto u8 = path.u8string();
    return std::all_of(u8.begin(), u8.end(), [](unsigned char ch) { return ch < 0x80; });
}

/**
 * @brief 交给 VTK 解析前的字节级预检与数据集类型探测
 *
 * 安全约束：文档中出现 DOCTYPE / ENTITY 声明（含内部子集）直接拒绝；
 * 同时在文件头部探测 VTKFile 的 type 属性，决定采用哪种 reader。
 */
enum class DatasetKind {
    Unsupported,
    PolyData,
    UnstructuredGrid,
};

DatasetKind preflightCheck(const std::string& bytes)
{
    if (bytes.find("<!DOCTYPE") != std::string::npos || bytes.find("<!ENTITY") != std::string::npos) {
        spdlog::error("VtkXmlModelHandler: DOCTYPE/ENTITY declaration is rejected");
        return DatasetKind::Unsupported;
    }
    // VTKFile 是根元素，type 属性必在文件头部
    const size_t head_size = std::min<size_t>(bytes.size(), 4096);
    const std::string head = bytes.substr(0, head_size);
    if (head.find("type=\"PolyData\"") != std::string::npos) {
        return DatasetKind::PolyData;
    }
    if (head.find("type=\"UnstructuredGrid\"") != std::string::npos) {
        return DatasetKind::UnstructuredGrid;
    }
    spdlog::error("VtkXmlModelHandler: unsupported VTKFile type or missing type attribute");
    return DatasetKind::Unsupported;
}

//! @brief 非 ASCII 路径时把文件复制到 ASCII 临时路径，返回给 VTK 用的实际路径
std::filesystem::path prepareAsciiPath(const std::filesystem::path& path, const std::string& extension)
{
    if (isAsciiPath(path)) {
        return path;
    }
    std::filesystem::path temp = core::TempFile::instance().path();
    temp.replace_extension(extension);
    std::error_code copy_error;
    std::filesystem::copy_file(path, temp, std::filesystem::copy_options::overwrite_existing, copy_error);
    if (copy_error) {
        spdlog::error("VtkXmlModelHandler: failed to stage non-ascii path '{}': {}",
            path.string(), copy_error.message());
        return {};
    }
    return temp;
}

//! @brief Points 展开为顶点数组，返回顶点数（0 视为读取失败）
bool readVertices(vtkPoints* points, MeshData& mesh)
{
    if (!points || points->GetNumberOfPoints() <= 0) {
        spdlog::error("VtkXmlModelHandler: dataset has no points");
        return false;
    }
    const vtkIdType point_count = points->GetNumberOfPoints();
    mesh.vertex_positions_.reserve(static_cast<size_t>(point_count));
    for (vtkIdType i = 0; i < point_count; ++i) {
        double position[3] {};
        points->GetPoint(i, position);
        mesh.vertex_positions_.push_back({ position[0], position[1], position[2] });
    }
    mesh.vertex_count_ = static_cast<Index>(mesh.vertex_positions_.size());
    return true;
}

//! @brief 单元的点 id 是否全部落在顶点范围内（VTK reader 不校验 connectivity，
//! 越界引用须在转换层拦截，避免垃圾 id 进入模型层）
bool idsWithinRange(vtkIdList* ids, vtkIdType vertex_count)
{
    const vtkIdType corner_count = ids->GetNumberOfIds();
    for (vtkIdType k = 0; k < corner_count; ++k) {
        if (ids->GetId(k) < 0 || ids->GetId(k) >= vertex_count) {
            spdlog::error("VtkXmlModelHandler: cell references point {} out of {}",
                ids->GetId(k), vertex_count);
            return false;
        }
    }
    return true;
}

//! @brief 追加一段折线为相邻点对的边单元（单点段静默忽略）；越界引用返回 false
bool appendPolyline(vtkIdList* ids, MeshData& mesh, vtkIdType vertex_count)
{
    if (!idsWithinRange(ids, vertex_count)) {
        return false;
    }
    const vtkIdType point_count = ids->GetNumberOfIds();
    for (vtkIdType k = 0; k + 1 < point_count; ++k) {
        mesh.edge_vertices_.push_back(static_cast<Index>(ids->GetId(k)));
        mesh.edge_vertices_.push_back(static_cast<Index>(ids->GetId(k + 1)));
    }
    return true;
}

//! @brief 逐段遍历 vtkCellArray（Lines/Polys 共用），对每段执行回调
template <typename Visit>
void forEachSegment(vtkCellArray* cells, Visit&& visit)
{
    if (!cells) {
        return;
    }
    vtkNew<vtkIdList> ids;
    const vtkIdType segment_count = cells->GetNumberOfCells();
    for (vtkIdType s = 0; s < segment_count; ++s) {
        cells->GetCellAtId(s, ids);
        visit(ids);
    }
}

//! @brief vtkPolyData -> MeshData：Lines -> 边、Polys -> 面；Verts/Strips 告警忽略
bool meshFromPolyData(vtkPolyData* poly_data, MeshData& mesh)
{
    if (!readVertices(poly_data->GetPoints(), mesh)) {
        return false;
    }
    const vtkIdType vertex_count = static_cast<vtkIdType>(mesh.vertex_count_);
    bool ids_ok = true;

    forEachSegment(poly_data->GetLines(), [&](vtkIdList* ids) {
        if (!appendPolyline(ids, mesh, vertex_count)) {
            ids_ok = false;
        }
    });
    forEachSegment(poly_data->GetPolys(), [&](vtkIdList* ids) {
        const vtkIdType corner_count = ids->GetNumberOfIds();
        if (corner_count < 3) {
            spdlog::warn("VtkXmlModelHandler: degenerate face with {} corners skipped", corner_count);
            return;
        }
        if (!idsWithinRange(ids, vertex_count)) {
            ids_ok = false;
            return;
        }
        for (vtkIdType k = 0; k < corner_count; ++k) {
            mesh.face_vertices_.push_back(static_cast<Index>(ids->GetId(k)));
        }
        mesh.face_vertices_offset_.push_back(static_cast<Index>(mesh.face_vertices_.size()));
    });
    if (poly_data->GetVerts() && poly_data->GetVerts()->GetNumberOfCells() > 0) {
        spdlog::warn("VtkXmlModelHandler: PolyData Verts are not mesh cells, ignored");
    }
    if (poly_data->GetStrips() && poly_data->GetStrips()->GetNumberOfCells() > 0) {
        spdlog::warn("VtkXmlModelHandler: PolyData Strips are not supported, ignored");
    }
    return ids_ok;
}

//! @brief vtkUnstructuredGrid -> MeshData：按 cell 维度分发到边/面/体
bool meshFromUnstructuredGrid(vtkUnstructuredGrid* grid, MeshData& mesh)
{
    if (!readVertices(grid->GetPoints(), mesh)) {
        return false;
    }
    const vtkIdType vertex_count = static_cast<vtkIdType>(mesh.vertex_count_);

    const vtkIdType cell_count = grid->GetNumberOfCells();
    for (vtkIdType ci = 0; ci < cell_count; ++ci) {
        vtkCell* cell = grid->GetCell(ci);
        if (!cell) {
            continue;
        }
        vtkIdList* ids = cell->GetPointIds();
        if (!ids || ids->GetNumberOfIds() <= 0) {
            continue;
        }
        const vtkIdType corner_count = ids->GetNumberOfIds();
        if (!idsWithinRange(ids, vertex_count)) {
            return false;
        }

        switch (cell->GetCellDimension()) {
        case 1:
            // 线段/折线 -> 相邻点对边单元
            if (!appendPolyline(ids, mesh, vertex_count)) {
                return false;
            }
            break;
        case 2:
            // 三角形/四边形/多边形 -> 面单元
            for (vtkIdType k = 0; k < corner_count; ++k) {
                mesh.face_vertices_.push_back(static_cast<Index>(ids->GetId(k)));
            }
            mesh.face_vertices_offset_.push_back(static_cast<Index>(mesh.face_vertices_.size()));
            break;
        case 3:
            // 体单元沿用 VTK 类型码；多面体（VTK_POLYHEDRON）需要面拓扑，跳过
            if (cell->GetCellType() == VTK_POLYHEDRON) {
                spdlog::warn("VtkXmlModelHandler: VTK_POLYHEDRON cell {} skipped", ci);
                break;
            }
            mesh.solid_types_.push_back(static_cast<unsigned char>(cell->GetCellType()));
            for (vtkIdType k = 0; k < corner_count; ++k) {
                mesh.solid_vertices_.push_back(static_cast<Index>(ids->GetId(k)));
            }
            mesh.solid_vertices_offset_.push_back(static_cast<Index>(mesh.solid_vertices_.size()));
            mesh.solid_faces_offset_.push_back(0);
            break;
        default:
            // 顶点单元（dim 0）不是网格单元，静默忽略
            break;
        }
    }
    return true;
}

/**
 * @brief 把一个组件的网格追加到 merged，多组件导出时按点偏移拼成一个网格
 *
 * MeshData 自包含、连通性存组件内局部点索引，追加时统一加 vertex_offset。
 * 面/边单元与体单元全量平移（角点数与类型码原样保留）。
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

    // 面单元：越界与退化（<3 角点）脏面整面跳过
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
            std::vector<Index> corners(static_cast<size_t>(end - begin));
            bool face_ok = true;
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

    // 体单元：类型码原样保留（写出侧由 VTK writer 解释）
    Index skipped_solids = 0;
    if (source->solid_vertices_offset_.size() >= 2
        && source->solid_types_.size() + 1 == source->solid_vertices_offset_.size()) {
        const Index solid_count = static_cast<Index>(source->solid_types_.size());
        const Index corner_count = static_cast<Index>(source->solid_vertices_.size());
        for (Index s = 0; s < solid_count; ++s) {
            const Index begin = source->solid_vertices_offset_[static_cast<size_t>(s)];
            const Index end = source->solid_vertices_offset_[static_cast<size_t>(s) + 1];
            if (begin < 0 || end > corner_count || end < begin) {
                ++skipped_solids;
                continue;
            }
            std::vector<Index> corners(static_cast<size_t>(end - begin));
            bool solid_ok = true;
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
            merged.solid_types_.push_back(source->solid_types_[static_cast<size_t>(s)]);
            merged.solid_vertices_.insert(merged.solid_vertices_.end(), corners.begin(), corners.end());
            merged.solid_vertices_offset_.push_back(static_cast<Index>(merged.solid_vertices_.size()));
            merged.solid_faces_offset_.push_back(0);
        }
    }
    if (skipped_solids > 0) {
        spdlog::warn("VtkXmlModelHandler: component {} has {} dirty solid(s), skip",
            component.id, skipped_solids);
    }

    vertex_offset += point_count;
    return true;
}

//! @brief 从合并网格构造顶点集（写路径共用）
vtkSmartPointer<vtkPoints> makePoints(const MeshData& mesh)
{
    vtkSmartPointer<vtkPoints> points = vtkSmartPointer<vtkPoints>::New();
    points->SetDataTypeToDouble();
    points->Allocate(static_cast<vtkIdType>(mesh.vertex_positions_.size()));
    for (const auto& position : mesh.vertex_positions_) {
        points->InsertNextPoint(position[0], position[1], position[2]);
    }
    return points;
}

//! @brief 写出 .vtp（PolyData）：Lines 承载边、Polys 承载面；体单元告警丢弃
bool writeVtp(const std::filesystem::path& path, const MeshData& mesh)
{
    vtkNew<vtkCellArray> lines;
    for (size_t e = 0; e + 1 < mesh.edge_vertices_.size(); e += 2) {
        const vtkIdType ids[2] = { static_cast<vtkIdType>(mesh.edge_vertices_[e]),
            static_cast<vtkIdType>(mesh.edge_vertices_[e + 1]) };
        lines->InsertNextCell(2, ids);
    }

    vtkNew<vtkCellArray> polys;
    if (mesh.face_vertices_offset_.size() >= 2) {
        for (size_t f = 0; f + 1 < mesh.face_vertices_offset_.size(); ++f) {
            const Index begin = mesh.face_vertices_offset_[f];
            const Index end = mesh.face_vertices_offset_[f + 1];
            std::vector<vtkIdType> ids(static_cast<size_t>(end - begin));
            for (Index c = begin; c < end; ++c) {
                ids[static_cast<size_t>(c - begin)] = static_cast<vtkIdType>(mesh.face_vertices_[static_cast<size_t>(c)]);
            }
            polys->InsertNextCell(static_cast<vtkIdType>(ids.size()), ids.data());
        }
    }
    if (!mesh.solid_types_.empty()) {
        spdlog::warn("VtkXmlModelHandler: {} solid cell(s) cannot be stored in PolyData, dropped",
            mesh.solid_types_.size());
    }

    vtkNew<vtkPolyData> poly_data;
    poly_data->SetPoints(makePoints(mesh));
    poly_data->SetLines(lines);
    poly_data->SetPolys(polys);

    vtkSmartPointer<vtkXMLPolyDataWriter> writer = vtkSmartPointer<vtkXMLPolyDataWriter>::New();
    writer->SetFileName(path.string().c_str());
    writer->SetInputData(poly_data);
    writer->SetDataModeToAscii();
    return writer->Write() == 1;
}

//! @brief 写出 .vtu（UnstructuredGrid）：边/面/体全量，体单元沿用 VTK 类型码
bool writeVtu(const std::filesystem::path& path, const MeshData& mesh)
{
    vtkNew<vtkUnstructuredGrid> grid;
    grid->SetPoints(makePoints(mesh));

    for (size_t e = 0; e + 1 < mesh.edge_vertices_.size(); e += 2) {
        const vtkIdType ids[2] = { static_cast<vtkIdType>(mesh.edge_vertices_[e]),
            static_cast<vtkIdType>(mesh.edge_vertices_[e + 1]) };
        grid->InsertNextCell(VTK_LINE, 2, ids);
    }
    if (mesh.face_vertices_offset_.size() >= 2) {
        for (size_t f = 0; f + 1 < mesh.face_vertices_offset_.size(); ++f) {
            const Index begin = mesh.face_vertices_offset_[f];
            const Index end = mesh.face_vertices_offset_[f + 1];
            const Index corner_count = end - begin;
            const int cell_type = corner_count == 3
                ? VTK_TRIANGLE
                : (corner_count == 4 ? VTK_QUAD : VTK_POLYGON);
            std::vector<vtkIdType> ids(static_cast<size_t>(corner_count));
            for (Index c = begin; c < end; ++c) {
                ids[static_cast<size_t>(c - begin)] = static_cast<vtkIdType>(mesh.face_vertices_[static_cast<size_t>(c)]);
            }
            grid->InsertNextCell(cell_type, static_cast<vtkIdType>(ids.size()), ids.data());
        }
    }
    for (size_t s = 0; s < mesh.solid_types_.size(); ++s) {
        const Index begin = mesh.solid_vertices_offset_[s];
        const Index end = mesh.solid_vertices_offset_[s + 1];
        std::vector<vtkIdType> ids(static_cast<size_t>(end - begin));
        for (Index c = begin; c < end; ++c) {
            ids[static_cast<size_t>(c - begin)] = static_cast<vtkIdType>(mesh.solid_vertices_[static_cast<size_t>(c)]);
        }
        grid->InsertNextCell(static_cast<int>(mesh.solid_types_[s]),
            static_cast<vtkIdType>(ids.size()), ids.data());
    }

    vtkSmartPointer<vtkXMLUnstructuredGridWriter> writer = vtkSmartPointer<vtkXMLUnstructuredGridWriter>::New();
    writer->SetFileName(path.string().c_str());
    writer->SetInputData(grid);
    writer->SetDataModeToAscii();
    return writer->Write() == 1;
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
        // VTK（expat）解析前的字节级预检：拒绝 DOCTYPE/ENTITY，探测数据集类型
        const std::string bytes = readAllBytes(path);
        if (bytes.empty()) {
            spdlog::error("VtkXmlModelHandler: failed to open file '{}'", path.string());
            return std::nullopt;
        }
        const DatasetKind kind = preflightCheck(bytes);
        if (kind == DatasetKind::Unsupported) {
            spdlog::error("VtkXmlModelHandler: failed to read VTK XML file: {}", path.string());
            return std::nullopt;
        }

        // 非 ASCII 路径经 ASCII 临时文件中转（vtkXMLReader 文件名为窄字符）
        const std::string extension = kind == DatasetKind::PolyData ? ".vtp" : ".vtu";
        const std::filesystem::path effective_path = prepareAsciiPath(path, extension);
        if (effective_path.empty()) {
            return std::nullopt;
        }

        bool ok = false;
        if (kind == DatasetKind::PolyData) {
            vtkSmartPointer<vtkXMLPolyDataReader> reader = vtkSmartPointer<vtkXMLPolyDataReader>::New();
            reader->SetFileName(effective_path.string().c_str());
            reader->Update();
            ok = meshFromPolyData(reader->GetOutput(), *mesh);
        } else {
            vtkSmartPointer<vtkXMLUnstructuredGridReader> reader
                = vtkSmartPointer<vtkXMLUnstructuredGridReader>::New();
            reader->SetFileName(effective_path.string().c_str());
            reader->Update();
            ok = meshFromUnstructuredGrid(reader->GetOutput(), *mesh);
        }
        if (!ok) {
            spdlog::error("VtkXmlModelHandler: failed to read VTK XML file: {}", path.string());
            return std::nullopt;
        }

        // 读入用的临时文件随会话清理，不保留
        if (effective_path != path) {
            std::error_code remove_error;
            std::filesystem::remove(effective_path, remove_error);
        }
    } catch (const std::exception& e) {
        spdlog::error("VtkXmlModelHandler: exception reading '{}': {}", path.string(), e.what());
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
    if (merged_count == 0 || merged.vertex_positions_.empty()) {
        spdlog::error("VtkXmlModelHandler: no mesh component to export");
        return;
    }
    if (!has_cells) {
        spdlog::warn("VtkXmlModelHandler: exporting point-only dataset (no cells)");
    }

    merged.vertex_count_ = static_cast<Index>(merged.vertex_positions_.size());

    // 非 ASCII 目标路径先写 ASCII 临时文件再复制回去
    std::filesystem::path effective_path = path;
    std::filesystem::path temp_out;
    if (!isAsciiPath(path)) {
        temp_out = core::TempFile::instance().path();
        temp_out.replace_extension(path.extension().empty() ? ".vtu" : path.extension());
        effective_path = temp_out;
    }

    const bool want_poly_data = path.extension() == ".vtp";
    const bool written = want_poly_data ? writeVtp(effective_path, merged) : writeVtu(effective_path, merged);
    if (!written) {
        spdlog::error("VtkXmlModelHandler: failed to write VTK XML file: {}", path.string());
        return;
    }
    if (!temp_out.empty()) {
        std::error_code copy_error;
        std::filesystem::copy_file(temp_out, path, std::filesystem::copy_options::overwrite_existing, copy_error);
        std::error_code remove_error;
        std::filesystem::remove(temp_out, remove_error);
        if (copy_error) {
            spdlog::error("VtkXmlModelHandler: failed to move output to '{}': {}",
                path.string(), copy_error.message());
            return;
        }
    }

    spdlog::info("VtkXmlModelHandler: wrote {} file: {} (components_merged={})",
        want_poly_data ? "vtp" : "vtu", path.string(), merged_count);
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
