#ifndef CORE_H
#define CORE_H
#include <vector>
#include <array>

enum class RenderMode {
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
using IndexVtk = int;

struct BlockData {
    std::vector<IndexVtk> faces_;
    Index model_id_;
};

struct BlockDatas {
    std::vector<BlockData> block_datas;
};

struct ModelDataVtk {
    std::vector<std::array<IndexVtk, 3>> vtk_triangles_;
    std::vector<std::array<double, 3>> vtk_points_;
    std::vector<Index> model_face_id_;
    std::vector<Index> model_point_id_;
    BlockDatas model_blocks_;


    Index model_face_id(IndexVtk face_id)
{

    return this->model_face_id_[face_id];
}
    Index model_point_id(IndexVtk point_id)
{
    return this->model_point_id_[point_id];
}
    Index model_block_id(IndexVtk block_id)
{
    return this->model_blocks_.block_datas[block_id].model_id_;
}
};
#endif // CORE_H