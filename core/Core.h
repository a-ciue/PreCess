#ifndef CORE_H
#define CORE_H
#include <vector>
#include <array>
#include <memory>

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
    Index id;
};

struct BlockDatas {
    std::vector<BlockData> block_datas;
};

struct MeshDataVtk {
    const std::vector<std::array<Index, 3>>& vtk_triangles_;
    const std::vector<std::array<double, 3>>& vtk_points_;
    std::shared_ptr<BlockDatas> model_blocks_;

    Index model_block_id(Index block_id) const
{
    return this->model_blocks_->block_datas[block_id].id;
}
};
#endif // CORE_H