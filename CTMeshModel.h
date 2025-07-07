#pragma once
#include <unordered_set>
#include <filesystem>

#include "MeshModel.h"
#include "Core.h"

namespace MeshLib {
    template <typename V, typename E, typename F, typename H>
    class CToolMesh;
    class CToolVertex;
    class CToolEdge;
    class CToolFace;
    class CToolHalfEdge;
    template class CToolMesh<CToolVertex, CToolEdge, CToolFace, CToolHalfEdge>;
    using CTMesh = CToolMesh<CToolVertex, CToolEdge, CToolFace, CToolHalfEdge>;
}

/**
 * @brief 模型层辅助数据结构 CTMesh
 */
class CTMeshModel : MeshModelBase {
public:
    CTMeshModel(const std::filesystem::path& mesh_path);

    /**
     * @brief 更新MeshData
     * @param mesh_data 待更新的核心网格数据结构
     *
     * 更新不只是简单重新构造对应的MeshData，需要利用MeshData的已有数据尽量达到增量更新的目的，提高更新效率
     * 已有数据包括：
     * - Patch如何组成Block，这一数据从文件中读取不来
     * - MeshData、Patch中的容器空间，包括MeshData::face_vertices, vertex_positions, Patch对象。
     *
     * 所以维护数据主要工作：
     * 1. 利用已有的Patch对象更新包含的面faces
     * 2. 更新已有的块信息，新增Patch要记录，删除Patch要清理
     * 3. 更新MeshData的face_vertices, vertex_positions等容器
     */
    void update(MeshData& mesh_data) override;
    /**
     * @brief 更新MeshData，只更新指定的Patch
     * @param mesh_data 待更新的核心网格数据结构
     * @param patch_ids 指定需要更新的Patch ID集合
     */
    void update(MeshData& mesh_data, const std::unordered_set<Index>& patch_ids);

private:
    std::unique_ptr<MeshLib::CTMesh> mesh_;
};
