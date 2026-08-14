#ifndef CORE_H
#define CORE_H
#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

enum class GeometryRenderStyle {
    SurfaceWithEdges,
    Surface,
    Transparent75,
    Transparent50,
    Transparent25,
    WireframeWithLines,
    Wireframe,
    Hidden
};

enum class MeshRenderStyle {
    FaceWithEdges,
    Face,
    Transparent75,
    Transparent50,
    Transparent25,
    WireframeInternal,
    WireframeSurface,
    Hidden
};

enum class SelectMode {
    None,
    Vertex,
    Face,
    Edge,
    Solid,
    Block,
    GeometryVertex,
    GeometryEdge,
    GeometryFace,
    GeometrySolid,
    Component
};

/**
 * @brief 基本索引类型定义
 */
using Index = int;

/**
 * @brief 一条可直接用于诊断渲染的网格边
 */
struct TopologyDiagnosticEdge {
    std::array<Index, 2> endpoints; //> 组件内局部点 id
    double dihedral_angle_degrees { -1.0 }; //> 无有效二面角时为 -1
};

/**
 * @brief 保存一次网格拓扑诊断得到的特殊实体集合
 */
struct MeshTopologyDiagnosticResult {
    std::vector<TopologyDiagnosticEdge> boundary_edges;
    std::vector<Index> boundary_faces;
    std::vector<TopologyDiagnosticEdge> non_manifold_edges;
    std::vector<Index> non_manifold_vertices;
    std::vector<TopologyDiagnosticEdge> isolated_edges;
    std::vector<Index> isolated_vertices;
    std::vector<TopologyDiagnosticEdge> manifold_edges; //> 恰好邻接两个面的边，保存二面角供范围筛选
};

using GeomVertexId = int;
using GeomEdgeId = int;
using GeomFaceId = int;
using GeomSolidId = int;

constexpr GeomVertexId kInvalidGeomVertexId = -1;
constexpr GeomEdgeId kInvalidGeomEdgeId = -1;
constexpr GeomFaceId kInvalidGeomFaceId = -1;
constexpr GeomSolidId kInvalidGeomSolidId = -1;

struct BlockData {
    std::vector<Index> faces_;
    Index id;
};

struct BlockDatas {
    std::vector<BlockData> block_datas;
};

/**
 * @brief 用于存储网格数据以供 VTK 渲染使用的结构体
 *
 * 详细描述见 MeshData 说明
 * @sa MeshData
 */
struct MeshDataVtk {
    const std::vector<unsigned char>& vtk_solid_cell_types_; //> 对应 MeshData::solid_types_
    const std::vector<Index>& vtk_solid_cells_; //> 对应 MeshData::solid_vertices_
    const std::vector<Index>& vtk_solid_cells_offset_; //> 对应 MeshData::solid_vertices_offset_
    const std::vector<Index>& vtk_solid_faces_; //> 对应 MeshData::solid_faces_vertices_
    const std::vector<Index>& vtk_solid_faces_offset_; //> 对应 MeshData::solid_faces_vertices_offset_
    const std::vector<Index>& vtk_solid_face_locations_; //> 对应 MeshData::solid_faces_
    const std::vector<Index>& vtk_solid_face_locations_offset_; //> 对应 MeshData::solid_faces_offset_

    const std::vector<Index>& vtk_face_cells_; //> 表示面顶点索引的数组，对应 MeshData::face_vertices_
    const std::vector<Index>& vtk_face_cells_offset_; //> 表示面顶点索引偏移的数组，对应 MeshData::face_vertices_offset_

    const std::vector<Index>& vtk_edge_cells_; //> 表示边顶点索引的数组，对应 MeshData::edge_vertices_

    const std::vector<std::array<double, 3>>& vertex_positions_; //> 对应 MeshData::vertex_positions_（组件自持坐标）

    const std::map<std::string, std::vector<double>>& vertex_attributes_; //> 表示点属性的数组，对应 MeshData::vertex_attributes_
    const std::map<std::string, std::vector<double>>& edge_attributes_; //> 表示边属性的数组，对应 MeshData::edge_attributes_
    const std::map<std::string, std::vector<double>>& face_attributes_; //> 表示面属性的数组，对应 MeshData::face_attributes_
    const std::map<std::string, std::vector<double>>& solid_attributes_; //> 表示体属性的数组，对应 MeshData::solid_attributes_

    std::shared_ptr<BlockDatas> model_blocks_;

    Index component_id { -1 };

    std::shared_ptr<const MeshTopologyDiagnosticResult> topology_diagnostics_; //> 只读派生诊断结果，供渲染层叠加显示

    Index model_block_id(Index block_id) const
    {
        return this->model_blocks_->block_datas[block_id].id;
    }
};
#endif // CORE_H
