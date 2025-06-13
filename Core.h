#ifndef CORE_H
#define CORE_H
#include <vector>
#include <array>
#include <TopoDS_Shape.hxx>

enum class ModelRenderMode {
    Face,
    Block,
    Group
};

enum class SplineRenderMode
{
    Face,
    Block,
    Group
};

enum class SelectMode {
	None,
    Face,
	Edge,
    Block
};

using Index = int;

struct BlockData {
    std::vector<Index> faces_;
    Index model_id_;
};

struct BlockDatas {
    std::vector<BlockData> block_datas;
};

struct MeshDataVtk {
    std::vector<std::array<Index, 3>> vtk_triangles_;
    std::vector<std::array<double, 3>> vtk_points_;
    std::vector<Index> model_face_id_;
    std::vector<Index> model_point_id_;
    BlockDatas model_blocks_;


    Index model_face_id(Index face_id) const
{

    return this->model_face_id_[face_id];
}
    Index model_point_id(Index point_id) const
{
    return this->model_point_id_[point_id];
}
    Index model_block_id(Index block_id) const
{
    return this->model_blocks_.block_datas[block_id].model_id_;
}
};

struct SplineDataVtk
{
    TopoDS_Shape shape;
};
#endif // CORE_H