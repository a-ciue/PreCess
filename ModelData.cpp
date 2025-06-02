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

    // 记录父节点信息
    int father_id = md->patches_.at(patch_id)->father_id;

    std::vector<int> affected_patch_ids = { patch_id };
    // 执行面切分操作
    ModelUtil::split_face(face, md->mesh_.get())->point() = mid;

    // 更新父节点信息
    for (int pid : affected_patch_ids) {
        update_father_id(pid, father_id);
    }
    md->update_patches(std::vector{ patch_id }, false);
    update_actors({ patch_id });
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

    std::vector<int> patch_ids {};
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
    update_actors(patch_ids);
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

    // 获取目标块的father_id
    int father_id = md->patches_[target_block->patchIDs.begin() != target_block->patchIDs.end() ?
        *(target_block->patchIDs.begin()) : 0]->father_id; // 从第一个patch获取father_id

    // 合并其他 block 的内容到目标 block
    std::unordered_set<int> modified_groups;
    for (size_t i = 1; i < block_ids.size(); ++i) {
        int id = block_ids[i];
        auto& block_to_merge = md->blocks_[id];

        // 合并 patchIDs
        for (int patch_id : block_to_merge->patchIDs) {
            target_block->patchIDs.insert(patch_id);
            md->patches_[patch_id]->blockID = target_block_id;
        
            // 更新父节点信息
            update_father_id(patch_id, father_id); // 更新每个patch的father_id
        }

        // 维护groups_，删除后面这些在group的信息
        modified_groups.insert(block_to_merge->groupID);
        Group& group = *md->groups_[block_to_merge->groupID];
        group.blockIDs.erase(block_to_merge->id);
        if (group.blockIDs.empty()) {
            md->groups_.erase(block_to_merge->groupID);
        }

        // 删除已合并的 block
        md->blocks_.erase(id);
    }

    // 更新 ModelActor
    // 更新目标 block 的 patchIDs
    // 调用 ModelActor 的 merge_blocks 函数更新 Actor
    //emit blocksMerged(getModelName(), block_ids, block_ids[0], target_block->patchIDs);
    //emit groupUpdated(getModelName(), target_block->groupID, groups_[target_block->groupID]->blockIDs);
    //for (int modified_group : modified_groups) {
    //    if (groups_.count(modified_group)) {
    //        emit groupUpdated(getModelName(), modified_group, groups_[modified_group]->blockIDs);
    //    } else {
    //        emit groupUpdated(getModelName(), modified_group, {});
    //    }
    //}
}

void ModelData::merge_groups(QSelection* selection) {
    auto* md = asMeshData();
    auto sel = selection->move();
    const std::vector<int>& group_ids = sel->ids;
    if (group_ids.empty()) {
        throw std::invalid_argument("group_ids cannot be empty.");
    }

    /* 验证 group_ids 是否有效
    for (int id : group_ids) {
        if (groups_.find(id) == groups_.end()) {
            throw std::runtime_error("Group ID not found: " + std::to_string(id));
        }
    }*/

    // 获取目标 group（第一个 group）
    int target_group_id = group_ids[0];
    auto& target_group = md->groups_[target_group_id];

    // 合并其他 group 的内容到目标 group
    for (size_t i = 1; i < group_ids.size(); ++i) {
        int id = group_ids[i];
        auto& group_to_merge = md->groups_[id];

        // 合并 blockIDs
        for (int block_id : group_to_merge->blockIDs) {
            target_group->blockIDs.insert(block_id);
            md->blocks_[block_id]->groupID = target_group_id;
        }

        // 删除已合并的 group
        md->groups_.erase(id);
    }

    // 更新 ModelActor
    //emit groupMerged(getModelName(), group_ids, group_ids[0], target_group->blockIDs);
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
    std::vector<int> all_patch_ids;
    for (auto& face : md->mesh_->faces()) {
        all_patch_ids.push_back(face->get_g());
    }

    // 更新 patches_
    md->update_patches(all_patch_ids, false);
}

void ModelData::remesh_group(QSelection* selection) {
    auto* md = asMeshData();
    auto sel = selection->move();
    const std::vector<int>& group_ids = sel->ids;
    // 收集所有 patch_ids
    std::unordered_set<int> patch_ids_set;
    for (int group_id : group_ids)
    {
        // 验证 group_id 是否有效
        if (md->groups_.find(group_id) == md->groups_.end()) {
            throw std::runtime_error("Group ID not found: " + std::to_string(group_id));
        }
	    
        // 获取指定 group
        auto& group = md->groups_[group_id];

        for (int block_id : group->blockIDs) {
            if (md->blocks_.find(block_id) == md->blocks_.end()) {
                throw std::runtime_error("Block ID not found in group: " + std::to_string(block_id));
            }
            auto& block = md->blocks_[block_id];
            patch_ids_set.insert(block->patchIDs.begin(), block->patchIDs.end());
        }
    }

    // 将 patch_ids_set 转换为 vector
    std::vector<int> patch_ids(patch_ids_set.begin(), patch_ids_set.end());

    // 调用 ModelUtil::remesh_patches 方法对这些 patch 进行重新网格化
    md->mesh_ = ModelUtil::remesh_patches(std::move(md->mesh_), patch_ids);

    // 更新所有patches
    std::vector<int> all_patch_ids;
    for (auto& face : md->mesh_->faces()) {
        all_patch_ids.push_back(face->get_g());
    }

    // 更新所有相关的 patches
    //update_patches(patch_ids, false);

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

int ModelData::block_group_id(int patch_id) {
    auto* md = asMeshData();
    // 先获取 patch 对应的 block_id
    int block_id = patch_block_id(patch_id);

    // 遍历 groups_ 查找包含 block_id 的 group
    for (const auto& [group_id, group_ptr] : md->groups_) {
        if (group_ptr->blockIDs.find(block_id) != group_ptr->blockIDs.end()) {
            return group_id; // 找到对应的 group_id
        }
    }

    // 如果找不到 block_id，抛出异常
    throw std::runtime_error("Block ID not found in any group.");
}

MeshDataVtk ModelData::getModelData()
{
    const auto* md = asMeshData();
    // 构造 ModelData
    MeshDataVtk modelData;

    // 添加所有顶点和三角形
    int offset{};
    unordered_map<int, vector<int>> patch_vtk_face_ids;
    for (const auto& [patch_id, patch] : md->patches_) {
        // 添加顶点和模型顶点ID
        modelData.vtk_points_.insert(modelData.vtk_points_.end(), patch->vertexPoints_.begin(), patch->vertexPoints_.end());
        modelData.model_point_id_.insert(modelData.model_point_id_.end(), patch->vertexIDs_.begin(), patch->vertexIDs_.end());
        modelData.model_face_id_.insert(modelData.model_face_id_.end(), patch->faceIDs_.begin(), patch->faceIDs_.end());

        // 添加三角形和模型面ID
        for (size_t i = 0; i < patch->faceTriangles_.size(); ++i) {
            array<Index, 3> arr;
            arr[0] = patch->faceTriangles_[i][0] + offset;
            arr[1] = patch->faceTriangles_[i][1] + offset;
            arr[2] = patch->faceTriangles_[i][2] + offset;
            modelData.vtk_triangles_.push_back(arr);
            patch_vtk_face_ids[patch_id].push_back(modelData.vtk_triangles_.size() - 1);
        }
        offset += patch->vertexPoints_.size();
    }

    // 添加所有块
    BlockDatas blockDatas;
    for (const auto& [block_id, block] : md->blocks_) {
        BlockData blockData;
        blockData.model_id_ = block_id;
        // 添加该块中所有的patch
        for (const auto& patch_id : block->patchIDs) {
            vector<int>& vtk_face_ids = patch_vtk_face_ids[patch_id];
            for (int vtk_face_id : vtk_face_ids)
            {
                blockData.faces_.push_back(vtk_face_id);
            }
        }
        blockDatas.block_datas.push_back(blockData);
    }
    modelData.model_blocks_ = blockDatas;

    return modelData;
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

void ModelData::update_actors(const std::vector<int>& patch_ids)
{
    std::unordered_set<int> block_ids, group_ids;
    for (int patch_id : patch_ids) {
        block_ids.insert(patch_block_id(patch_id));
        // emit patchUpdated(getModelName(), patch_id, patches_[patch_id]->vertexPoints_, patches_[patch_id]->faceTriangles_);
    }
    for (int block_id : block_ids) {
        group_ids.insert(block_group_id(block_id));
        // emit blockUpdated(getModelName(), block_id, blocks_[block_id]->patchIDs);
    }
}

void ModelData::update_father_id(int patch_id, int father_id) {
    auto* md = asMeshData();
    // 记录父节点id与子节点patch的映射
    auto& patch = md->patches_[patch_id];
    patch->father_id = father_id;
}

// 优化 update_patches 的实现，减少网格遍历次数
void ModelData::update_patches(const std::vector<int>& patch_ids, bool new_patch) {
    // 使用 unordered_set 来处理 patch_ids 的快速查找
    std::unordered_set<int> patch_id_set(patch_ids.begin(), patch_ids.end());

    // 调用重载函数
    update_patches(patch_id_set, new_patch);
}
void ModelData::update_patches(const std::unordered_set<int>& patch_ids, bool new_patch) {
    // 删除指定的 Patch 数据，但保持Patch所在的BlockID
    auto* md = asMeshData();
    std::unordered_map<int, int> blockIDs;
    for (int patch_id : patch_ids) {
        if (!new_patch && !md->patches_.count(patch_id))
        {
            //throw exception(("patch not found" + std::to_string(patch_id)).c_str());
            throw std::runtime_error("patch not found" + std::to_string(patch_id));
        }

        if (md->patches_.count(patch_id))
        {
            blockIDs[patch_id] = md->patches_[patch_id]->blockID;
            md->patches_.erase(patch_id);
        }
    }

    // 分组面片：按 Patch ID 将面片分组，取patch_ids包括的patch
    std::unordered_map<int, std::vector<MeshLib::CTMesh::CFace*>> patch_faces;
    for (auto& face : md->mesh_->faces()) {
        int face_patch_id = face->get_g();
        if (patch_ids.find(face_patch_id) != patch_ids.end()) {
            patch_faces[face_patch_id].push_back(face);
        }
    }

    // 遍历每个 Patch 的面片组
    for (const auto& [patch_id, faces] : patch_faces) {
        // 初始化 Patch
        auto& patch = md->patches_[patch_id];
        if (!patch) {
            patch = std::make_unique<Patch>();
            patch->id_ = patch_id;

            // 赋patch->blockID，从blockIDs取出
            if (!blockIDs.count(patch_id))
                blockIDs[patch_id] = -1;
            patch->blockID = blockIDs[patch_id];
            blockIDs.erase(patch_id);
        }

        // 追踪 Patch 内部的顶点
        std::unordered_map<int, int> vertex_id_map;  // 全局顶点 ID 到 Patch 内局部索引的映射

        // 遍历面片，更新 Patch 的顶点和三角形信息
        for (auto* face : faces) {
            // 添加当前面的三角形
            std::array<int, 3> triangle{};
            int i = 0;

            // 遍历面的顶点
            for (MeshLib::CTMesh::FaceVertexIterator vi(face); !vi.end(); ++vi) {
                auto vertex = *vi;

                // 如果顶点未被记录，添加到 Patch 的顶点列表
                if (vertex_id_map.find(vertex->id()) == vertex_id_map.end()) {
                    vertex_id_map[vertex->id()] = patch->vertexIDs_.size();
                    patch->vertexIDs_.push_back(vertex->id());

                    // 插入顶点坐标
                    CPoint& vp = vertex->point();
                    patch->vertexPoints_.emplace_back(std::array<double, 3>{vp[0], vp[1], vp[2]});
                }

                // 设置三角形索引
                triangle[i++] = vertex_id_map[vertex->id()];
            }

            // 添加三角形到 Patch 的索引列表
            patch->faceTriangles_.push_back(triangle);
            patch->faceIDs_.push_back(face->id());
        }
    }
}