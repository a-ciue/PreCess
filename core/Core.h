#ifndef CORE_H
#define CORE_H
#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>
enum class ModelRenderMode {
    Face,
    Block,
    Group
};

enum class GeometryRenderMode {
    Face,
    Block,
    Group
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
    GeometrySolid
};

/**
 * @brief 基本索引类型定义
 */
using Index = int;

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

    const std::map<std::string, std::vector<double>>& vertex_attributes_; //> 表示点属性的数组，对应 MeshData::vertex_attributes_
    const std::map<std::string, std::vector<double>>& edge_attributes_; //> 表示边属性的数组，对应 MeshData::edge_attributes_
    const std::map<std::string, std::vector<double>>& face_attributes_; //> 表示面属性的数组，对应 MeshData::face_attributes_
    const std::map<std::string, std::vector<double>>& solid_attributes_; //> 表示体属性的数组，对应 MeshData::solid_attributes_

    std::shared_ptr<BlockDatas> model_blocks_;

    Index component_id { -1 };

    Index model_block_id(Index block_id) const
    {
        return this->model_blocks_->block_datas[block_id].id;
    }
};
#endif // CORE_H