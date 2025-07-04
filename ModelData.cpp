/**
 * @file ModelData.cpp
 * @brief 实现 ModelData 类的核心功能，用于管理和操作网格数据
 *
 * 该文件包含 ModelData 类的实现，提供网格数据的存储、更新和操作功能，包括：
 * - 读取和写入网格数据
 * - 面和边的分割
 * - 块和组的合并
 * - 重新网格化功能
 * - 维护与 ModelActor 及 VTK 组件的交互
 *
 * @author 徐昊阳 haoyangxu06@gmail.com
 * @date 2025/3/8
 */

#include "ModelData.h"
#include "ToolMesh.h"
#include "ModelUtil.h"

#include <stdexcept>  // 用于抛出异常

ModelData::ModelData(MeshData mesh)
        : type_(Type::Mesh), data_(std::move(mesh))
{}

ModelData::ModelData(SplineData spline)
        : type_(Type::Spline), data_(std::move(spline))
{}

ModelData::Type ModelData::type() const { return type_; }
bool ModelData::isMesh()   const noexcept { return type_ == Type::Mesh; }
bool ModelData::isSpline() const noexcept { return type_ == Type::Spline; }

MeshData* ModelData::asMeshData() noexcept {
    return std::get_if<MeshData>(&data_);
}
const MeshData* ModelData::asMeshData() const noexcept {
    return std::get_if<MeshData>(&data_);
}

SplineData* ModelData::asSplineData() noexcept {
    return std::get_if<SplineData>(&data_);
}
const SplineData* ModelData::asSplineData() const noexcept {
    return std::get_if<SplineData>(&data_);
}

void ModelData::write_mesh(const std::filesystem::path& mesh_path, ModelRenderMode mode, const QString &extension)
{
    std::function<int(int)> gid{};
    auto* md = asMeshData();

    if (!md)
    {
        std::cerr << "call write mesh but not a mesh\n";
        return;
    }

    switch (mode) {
    case ModelRenderMode::Face:
        {
        gid = [](int patch_id) {
            return 1;
        };
        break;
    }
    case ModelRenderMode::Block: {
        auto* md = this->asMeshData();
        gid = [md](int patch_id) {
            //const auto& patch = md->patches_.at(patch_id);
            return md->blocks_[md->patches_[patch_id]->blockID]->id;
        };
        break;
    }
    }

    if (extension == "obj")
        ModelUtil::write_group_obj(md->mesh_.get(), mesh_path, gid);
    else if (extension == "inp")
        ModelUtil::write_group_inp(md->mesh_.get(), mesh_path, gid);
    else
        //"不支持的文件类型"
        assert(false);
}

void ModelData::split_face(QSelection* selection)
{
    auto* md = asMeshData();
    // 从 selection 中取出 Selection 对象
    auto sel = selection->move();
    // 假定 sel->ids[0] 为 patch_id，sel->ids[1] 为 face_id
    int face_id = sel->ids[0];
    int patch_id = md->mesh_->idFace(face_id)->get_g();
    //int face_gid = patches_[patch_id]->faceIDs_[face_id];
    int face_gid = face_id;
    MeshLib::CToolFace* face = md->mesh_->idFace(face_gid);

    CPoint mid;
    int i = 0;
    for (MeshLib::CTMesh::FaceVertexIterator vi(face); !vi.end(); vi++)
    {
        mid += vi.value()->point();
        ++i;
    }
    mid /= i;

    std::vector<int> affected_patch_ids = { patch_id };
    // 执行面切分操作
    ModelUtil::split_face(face, md->mesh_.get())->point() = mid;

    md->update_patches(std::vector{ patch_id }, false);
}

void ModelData::split_edge(QSelection* selection)
{
    auto* md = asMeshData();
    auto sel = selection->move();
    // 假定 sel->ids[0] 为 patch_id，sel->ids[1] 为 edge_v_id1，sel->ids[2] 为 edge_v_id2
    //int patch_id = sel->ids[0];
    int edge_v_id1 = sel->ids[0];
    int edge_v_id2 = sel->ids[1];
    //std::vector<int>& vids = patches_.at(patch_id)->vertexIDs_;
    //std::array<int, 2> edge_v_gid { vids[edge_v_ids[0]], vids[edge_v_ids[1]] };
    std::array<int, 2> edge_v_gid { edge_v_id1, edge_v_id2 };
    MeshLib::CToolVertex *v1 = md->mesh_->idVertex(edge_v_gid[0]),
                         *v2 = md->mesh_->idVertex(edge_v_gid[1]);

    MeshLib::CToolEdge* edge {};
    for (MeshLib::CTMesh::VertexEdgeIterator ei(v1); !ei.end(); ei++)
    {
        MeshLib::CToolEdge* ce = *ei;
	    if (md->mesh_->edgeVertex1(ce) == v2 || md->mesh_->edgeVertex2(ce) == v2)
	    {
            edge = ce;
		    break;
	    }
    }

    if (!edge)
    {
        throw std::runtime_error("invalid vertex id pair");
    }

    // 记录父节点信息
    //int father_id = patches_.at(patch_id)->father_id;

    ModelUtil::split_edge(edge, md->mesh_.get());

    std::vector<Index> patch_ids {};
    MeshLib::CToolHalfEdge* he1 = md->mesh_->edgeHalfedge(edge, 0);
    patch_ids.push_back(md->mesh_->halfedgeFace(he1)->get_g());
    if (MeshLib::CToolHalfEdge* he2 = md->mesh_->edgeHalfedge(edge, 1);
        he2 && md->mesh_->halfedgeFace(he2)->get_g() != patch_ids.front())
    // 被切分edge两侧不同属一个patch
    {
        patch_ids.push_back(md->mesh_->halfedgeFace(he2)->get_g());
    }

    // 更新涉及的patch的father_id
    //for (int pid : patch_ids) {
    //    update_father_id(pid, father_id);
    //}

    md->update_patches(patch_ids, false);
}

void ModelData::merge_blocks(QSelection* selection) {
    auto* md = asMeshData();
    auto sel = selection->move();
    const std::vector<int>& block_ids = sel->ids;
    if (block_ids.empty()) {
        throw std::invalid_argument("block_ids cannot be empty.");
    }

    /*验证 block_ids 是否有效
    for (int id : block_ids) {
        if (blocks_.find(id) == blocks_.end()) {
            throw std::runtime_error("Block ID not found: " + std::to_string(id));
        }
    }*/

    // 获取目标 block（第一个 block）
    int target_block_id = block_ids[0];
    auto& target_block = md->blocks_[target_block_id];

    // 合并其他 block 的内容到目标 block
    for (size_t i = 1; i < block_ids.size(); ++i) {
        int id = block_ids[i];
        auto& block_to_merge = md->blocks_[id];

        // 合并 patchIDs
        for (int patch_id : block_to_merge->patchIDs) {
            target_block->patchIDs.insert(patch_id);
            md->patches_[patch_id]->blockID = target_block_id;
        }
        // 删除已合并的 block
        md->blocks_.erase(id);
    }
}

void ModelData::remesh_block(QSelection* selection) {
    auto* md = asMeshData();
    auto sel = selection->move();
    const std::vector<int>& block_ids = sel->ids;
    // 验证 block_id 是否有效
    std::unordered_set<int> patch_ids_set;
    for (int block_id : block_ids) {
        if (md->blocks_.find(block_id) == md->blocks_.end()) {
            throw std::runtime_error("Block ID not found in group: " + std::to_string(block_id));
        }
        auto& block = md->blocks_[block_id];
        patch_ids_set.insert(block->patchIDs.begin(), block->patchIDs.end());
    }

    // 将 patch_ids_set 转换为 vector
    std::vector<int> patch_ids(patch_ids_set.begin(), patch_ids_set.end());

    // 调用 ModelUtil::remesh_patches 方法对 patch 重新网格化
    md->mesh_ = ModelUtil::remesh_patches(std::move(md->mesh_), patch_ids);

    // 更新所有相关的 patches
    //update_patches(patch_ids, false);

    // 更新所有patches
    std::vector<Index> all_patch_ids;
    for (auto& face : md->mesh_->faces()) {
        all_patch_ids.push_back(face->get_g());
    }

    // 更新 patches_
    md->update_patches(all_patch_ids, false);
}

int ModelData::face_patch_id(int face_id) {
    auto* md = asMeshData();
    // 遍历所有 patches
    for (const auto& [patch_id, patch_ptr] : md->patches_) {
        if (std::find(patch_ptr->faceIDs_.begin(), patch_ptr->faceIDs_.end(), face_id) != patch_ptr->faceIDs_.end()) {
            return patch_id; // 找到对应的 patch_id
        }
    }

    // 如果找不到 face_id，抛出异常或返回特殊值
    throw std::runtime_error("Face ID not found in any patch.");
}

const std::vector<int>& ModelData::patch_face_ids(int patch_id) {
    auto* md = asMeshData();
    // 检查 patch_id 是否存在
    if (md->patches_.find(patch_id) == md->patches_.end()) {
        throw std::runtime_error("Patch ID not found: " + std::to_string(patch_id));
    }

    // 返回对应 patch 的 faceIDs_
    return md->patches_[patch_id]->faceIDs_;
}

const std::vector<int>& ModelData::patch_vertex_ids(int patch_id) {
    auto* md = asMeshData();
    // 检查 patch_id 是否存在
    if (md->patches_.find(patch_id) == md->patches_.end()) {
        throw std::runtime_error("Patch ID not found: " + std::to_string(patch_id));
    }

    // 返回对应 patch 的 vertexIDs_
    return md->patches_[patch_id]->vertexIDs_;
}

int ModelData::patch_block_id(int patch_id) {
    auto* md = asMeshData();
    // 遍历 blocks_ 查找包含 patch_id 的 block
    for (const auto& [block_id, block_ptr] : md->blocks_) {
        if (block_ptr->patchIDs.find(patch_id) != block_ptr->patchIDs.end()) {
            return block_id; // 找到对应的 block_id
        }
    }

    // 如果找不到 patch_id，抛出异常
    throw std::runtime_error("Patch ID not found in any block.");
}

std::optional<SplineDataVtk> ModelData::getSplineData()
{
    const auto* md = asSplineData();
    if (md) {
        SplineDataVtk modelData{ md->rootShape };
        return modelData;
    }
    return nullopt;
}
