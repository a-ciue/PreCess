#include "Model.h"
#include "ModelActor.h"
#include "ToolMesh.h"
#include "ModelUtil.h"
#include <TDF_ChildIterator.hxx>
#include <TDataStd_Integer.hxx>

#include <stdexcept>  // 用于抛出异常

Model::Model(std::unique_ptr<MeshLib::CTMesh> mesh) : mesh_(std::move(mesh)) {
    doc = new TDocStd_Document("ModelDoc");
    patches_label = doc->Main().FindChild(1, Standard_True);
    blocks_label = doc->Main().FindChild(2, Standard_True);
    groups_label = doc->Main().FindChild(3, Standard_True);
}

void Model::write_mesh(const std::filesystem::path& mesh_path, ModelActor::RenderMode mode) {
    std::function<int(int)> gid{};

    switch (mode) {
        case ModelActor::RenderMode::Face:
            gid = [](int patch_id) {
                return 1;
            };
            break;
        case ModelActor::RenderMode::Block:
            gid = [this](int patch_id) {
                return GetBlockLabel(GetPatchLabel(patch_id).Tag()).Tag();
            };
            break;
        case ModelActor::RenderMode::Group:
            gid = [this](int patch_id) {
                return GetGroupLabel(GetBlockLabel(GetPatchLabel(patch_id).Tag()).Tag()).Tag();
            };
            break;
    }

    ModelUtil::write_group_obj(mesh_.get(), mesh_path, gid);
}

void Model::split_face(int patch_id, int face_id) {
    TDF_Label patch_label = GetPatchLabel(patch_id);

    CPoint mid;
    int i = 0;
    for (MeshLib::CTMesh::FaceVertexIterator vi(mesh_->idFace(face_id)); !vi.end(); vi++) {
        mid += vi.value()->point();
        ++i;
    }
    mid /= i;

    ModelUtil::split_face(mesh_->idFace(face_id), mesh_.get())->point() = mid;

    update_patches(std::unordered_set<int>{patch_id});  // 显式指定参数类型
    update_actors({patch_id});
}

void Model::split_edge(int patch_id, std::array<int, 2> edge_v_ids) {
    TDF_Label patch_label = GetPatchLabel(patch_id);

    std::array<int, 2> edge_v_gid = {edge_v_ids[0], edge_v_ids[1]};
    MeshLib::CToolVertex *v1 = mesh_->idVertex(edge_v_gid[0]),
            *v2 = mesh_->idVertex(edge_v_gid[1]);

    MeshLib::CToolEdge* edge {};
    for (MeshLib::CTMesh::VertexEdgeIterator ei(v1); !ei.end(); ei++) {
        MeshLib::CToolEdge* ce = *ei;
        if (mesh_->edgeVertex1(ce) == v2 || mesh_->edgeVertex2(ce) == v2) {
            edge = ce;
            break;
        }
    }

    if (!edge) {
        throw std::runtime_error("Invalid vertex id pair");
    }
    ModelUtil::split_edge(edge, mesh_.get());

    update_patches(std::unordered_set<int>{patch_id});  // 显式指定参数类型
    update_actors({patch_id});
}

void Model::merge_blocks(const std::vector<int>& block_ids) {
    if (block_ids.empty()) {
        throw std::invalid_argument("block_ids cannot be empty.");
    }

    TDF_Label target_block_label = GetBlockLabel(block_ids[0]);

    for (size_t i = 1; i < block_ids.size(); ++i) {
        TDF_Label block_label = GetBlockLabel(block_ids[i]);

        // Merge patches into target block
        for (TDF_ChildIterator it(block_label); it.More(); it.Next()) {
            TDF_Label patch_label = it.Value();
            patch_label.ForgetAllAttributes();
            Handle(TDataStd_Integer) blockIDAttr = TDataStd_Integer::Set(patch_label, block_ids[0]);
        }

        block_label.ForgetAllAttributes();
    }

    update_actors({block_ids[0]});
}

void Model::merge_groups(const std::vector<int>& group_ids) {
    if (group_ids.empty()) {
        throw std::invalid_argument("group_ids cannot be empty.");
    }

    TDF_Label target_group_label = GetGroupLabel(group_ids[0]);

    for (size_t i = 1; i < group_ids.size(); ++i) {
        TDF_Label group_label = GetGroupLabel(group_ids[i]);

        // Merge blocks into target group
        for (TDF_ChildIterator it(group_label); it.More(); it.Next()) {
            TDF_Label block_label = it.Value();
            block_label.ForgetAllAttributes();
            Handle(TDataStd_Integer) groupIDAttr = TDataStd_Integer::Set(block_label, group_ids[0]);
        }

        group_label.ForgetAllAttributes();
    }

    update_actors({group_ids[0]});
}

void Model::update_patches(const std::unordered_set<int>& patch_ids) {
    for (int patch_id : patch_ids) {
        TDF_Label patch_label = GetPatchLabel(patch_id);
        patch_label.ForgetAllAttributes(); // Reset attributes

        // Recalculate patch attributes
        Handle(TDataStd_Integer) patchIDAttr = TDataStd_Integer::Set(patch_label, patch_id);
    }
}

TDF_Label Model::GetPatchLabel(int patchID) {
    return patches_label.FindChild(patchID, Standard_True);
}

TDF_Label Model::GetBlockLabel(int blockID) {
    return blocks_label.FindChild(blockID, Standard_True);
}

TDF_Label Model::GetGroupLabel(int groupID) {
    return groups_label.FindChild(groupID, Standard_True);
}

ModelActor& Model::actor() {
    if (!actor_) {
        throw std::runtime_error("ModelActor is not initialized.");
    }
    return *actor_;
}

void Model::remesh_block(const std::vector<int>& block_ids) {
    std::unordered_set<int> patch_ids_set;
    for (int block_id : block_ids) {
        TDF_Label block_label = GetBlockLabel(block_id);

        for (TDF_ChildIterator it(block_label); it.More(); it.Next()) {
            TDF_Label patch_label = it.Value();
            patch_ids_set.insert(patch_label.Tag());
        }
    }

    std::vector<int> patch_ids(patch_ids_set.begin(), patch_ids_set.end());
    mesh_ = ModelUtil::remesh_patches(std::move(mesh_), patch_ids);

    update_patches(patch_ids);
    update_actors(patch_ids);
}

void Model::remesh_group(const std::vector<int>& group_ids) {
    std::unordered_set<int> patch_ids_set;
    for (int group_id : group_ids) {
        TDF_Label group_label = GetGroupLabel(group_id);

        for (TDF_ChildIterator block_it(group_label); block_it.More(); block_it.Next()) {
            TDF_Label block_label = block_it.Value();

            for (TDF_ChildIterator patch_it(block_label); patch_it.More(); patch_it.Next()) {
                TDF_Label patch_label = patch_it.Value();
                patch_ids_set.insert(patch_label.Tag());
            }
        }
    }

    std::vector<int> patch_ids(patch_ids_set.begin(), patch_ids_set.end());
    mesh_ = ModelUtil::remesh_patches(std::move(mesh_), patch_ids);

    update_patches(patch_ids);
    update_actors(patch_ids);
}

void Model::update_actors(const std::vector<int>& patch_ids) {
    std::unordered_set<int> block_ids, group_ids;

    // 更新 Patch 的 Actor
    for (int patch_id : patch_ids) {
        TDF_Label patch_label = GetPatchLabel(patch_id);

        // 获取 Patch 几何信息（从 update_patches 的逻辑移植）
        std::vector<MeshLib::CTMesh::CFace*> faces;
        for (auto& face : mesh_->faces()) {
            if (face->get_g() == patch_id) {
                faces.push_back(face);
            }
        }

        if (faces.empty()) continue;

        std::unordered_map<int, int> vertex_id_map;
        std::vector<std::array<double, 3>> points;
        std::vector<std::array<int, 3>> triangles;

        for (auto* face : faces) {
            std::array<int, 3> triangle {};
            int i = 0;

            for (MeshLib::CTMesh::FaceVertexIterator vi(face); !vi.end(); ++vi) {
                auto vertex = *vi;

                if (vertex_id_map.find(vertex->id()) == vertex_id_map.end()) {
                    vertex_id_map[vertex->id()] = points.size();
                    CPoint& vp = vertex->point();
                    points.emplace_back(std::array<double, 3>{vp[0], vp[1], vp[2]});
                }

                triangle[i++] = vertex_id_map[vertex->id()];
            }

            triangles.push_back(triangle);
        }

        // 更新 Actor
        actor_->update_patch(patch_label, points, triangles);

        // 获取 Block 标签
        block_ids.insert(GetBlockLabel(patch_id).Tag());
    }

    // 更新 Block 的 Actor
    for (int block_id : block_ids) {
        TDF_Label block_label = GetBlockLabel(block_id);
        actor_->update_block(block_label);

        group_ids.insert(GetGroupLabel(block_id).Tag());
    }

    // 更新 Group 的 Actor
    for (int group_id : group_ids) {
        TDF_Label group_label = GetGroupLabel(group_id);
        actor_->update_group(group_label);
    }
}

int Model::face_patch_id(int face_id) {
    for (auto& face : mesh_->faces()) {
        if (face->id() == face_id) {
            return face->get_g();
        }
    }
    throw std::runtime_error("Face ID not found in any patch.");
}

std::vector<int> Model::patch_face_ids(int patch_id) {
    TDF_Label patch_label = GetPatchLabel(patch_id);
    std::vector<int> face_ids;
    for (TDF_ChildIterator it(patch_label); it.More(); it.Next()) {
        face_ids.push_back(it.Value().Tag());
    }
    return face_ids;
}

std::vector<int> Model::patch_vertex_ids(int patch_id) {
    TDF_Label patch_label = GetPatchLabel(patch_id);
    std::vector<int> vertex_ids;
    for (TDF_ChildIterator it(patch_label); it.More(); it.Next()) {
        vertex_ids.push_back(it.Value().Tag());
    }
    return vertex_ids;
}

int Model::patch_block_id(int patch_id) {
    TDF_Label patch_label = GetPatchLabel(patch_id);
    return patch_label.Father().Tag();
}

int Model::block_group_id(int patch_id) {
    TDF_Label block_label = GetBlockLabel(patch_block_id(patch_id));
    return block_label.Father().Tag();
}
// 优化 update_patches 的实现，减少网格遍历次数

void Model::update_patches(const std::vector<int>& patch_ids) {
    // 使用 unordered_set 来处理 patch_ids 的快速查找
    std::unordered_set<int> patch_id_set(patch_ids.begin(), patch_ids.end());

    // 调用重载函数
    update_patches(patch_id_set);
}


///*
// * Model构造函数中需要调用ModelActor的构造函数和析构函数
// * 行17：In template: calling a private destructor of class 'ModelActor'
// * 行57：In template: calling a private constructor of class 'ModelActor'
// * 目前ModelActor的构造、析构函数仍为private
// * 一个解决方法：将ModelActor的构造、析构函数设置为public
//*/
//#include "Model.h"
//#include "ModelActor.h"
//#include "ToolMesh.h"
//#include "ModelUtil.h"
//
//#include <stdexcept>  // 用于抛出异常
//
//Model::Model(std::unique_ptr<MeshLib::CTMesh> mesh) : mesh_(std::move(mesh)){
//    doc = new TDocStd_Document("ModelDoc");
//}

//
//void Model::write_mesh(const std::filesystem::path& mesh_path, ModelActor::RenderMode mode)
//{
//    std::function<int(int)> gid{};
//
//    switch (mode) {
//    case ModelActor::RenderMode::Face:
//        {
//        gid = [](int patch_id) {
//            return 1;
//        };
//        break;
//    }
//    case ModelActor::RenderMode::Block: {
//        gid = [this](int patch_id) {
//            return blocks_[patches_[patch_id]->blockID]->id;
//        };
//        break;
//    }
//    case ModelActor::RenderMode::Group: {
//        gid = [this](int patch_id) {
//            return groups_[blocks_[patches_[patch_id]->blockID]->groupID]->id;
//        };
//        break;
//    }
//    }
//
//    ModelUtil::write_group_obj(mesh_.get(), mesh_path, gid);
//}
//
//void Model::split_face(int patch_id, int face_id)
//{
//    int face_gid = patches_[patch_id]->faceIDs_[face_id];
//    MeshLib::CToolFace* face = mesh_->idFace(face_gid);
//
//    CPoint mid;
//    int i = 0;
//    for (MeshLib::CTMesh::FaceVertexIterator vi(face); !vi.end(); vi++)
//    {
//        mid += vi.value()->point();
//        ++i;
//    }
//    mid /= i;
//
//    ModelUtil::split_face(face, mesh_.get())->point() = mid;
//
//    update_patches(std::vector{ patch_id });
//    update_actors({ patch_id });
//}
//
//void Model::split_edge(int patch_id, std::array<int, 2> edge_v_ids)
//{
//    std::vector<int>& vids = patches_[patch_id]->vertexIDs_;
//    std::array<int, 2> edge_v_gid { vids[edge_v_ids[0]], vids[edge_v_ids[1]] };
//    MeshLib::CToolVertex *v1 = mesh_->idVertex(edge_v_gid[0]),
//                         *v2 = mesh_->idVertex(edge_v_gid[1]);
//
//    MeshLib::CToolEdge* edge {};
//    for (MeshLib::CTMesh::VertexEdgeIterator ei(v1); !ei.end(); ei++)
//    {
//        MeshLib::CToolEdge* ce = *ei;
//	    if (mesh_->edgeVertex1(ce) == v2 || mesh_->edgeVertex2(ce) == v2)
//	    {
//            edge = ce;
//		    break;
//	    }
//    }
//
//    if (!edge)
//    {
//        throw std::runtime_error("invalid vertex id pair");
//    }
//    ModelUtil::split_edge(edge, mesh_.get());
//
//    std::vector<int> patch_ids {};
//    MeshLib::CToolHalfEdge* he1 = mesh_->edgeHalfedge(edge, 0);
//    patch_ids.push_back(mesh_->halfedgeFace(he1)->get_g());
//    if (MeshLib::CToolHalfEdge* he2 = mesh_->edgeHalfedge(edge, 1);
//        he2 && mesh_->halfedgeFace(he2)->get_g() != patch_ids.front())
//    // 被切分edge两侧不同属一个patch
//    {
//        patch_ids.push_back(mesh_->halfedgeFace(he2)->get_g());
//    }
//    update_patches(patch_ids);
//    update_actors(patch_ids);
//}
//
//void Model::merge_blocks(const std::vector<int>& block_ids) {
//    if (block_ids.empty()) {
//        throw std::invalid_argument("block_ids cannot be empty.");
//    }
//
//    /*验证 block_ids 是否有效
//    for (int id : block_ids) {
//        if (blocks_.find(id) == blocks_.end()) {
//            throw std::runtime_error("Block ID not found: " + std::to_string(id));
//        }
//    }*/
//
//    // 获取目标 block（第一个 block）
//    int target_block_id = block_ids[0];
//    auto& target_block = blocks_[target_block_id];
//
//    // 合并其他 block 的内容到目标 block
//    std::unordered_set<int> modified_groups;
//    for (size_t i = 1; i < block_ids.size(); ++i) {
//        int id = block_ids[i];
//        auto& block_to_merge = blocks_[id];
//
//        // 合并 patchIDs
//        for (int patch_id : block_to_merge->patchIDs) {
//            target_block->patchIDs.insert(patch_id);
//            patches_[patch_id]->blockID = target_block_id;
//        }
//
//        // 维护groups_，删除后面这些在group的信息
//        modified_groups.insert(block_to_merge->groupID);
//        Group& group = *groups_[block_to_merge->groupID];
//        group.blockIDs.erase(block_to_merge->id);
//        if (group.blockIDs.empty()) {
//            groups_.erase(block_to_merge->groupID);
//        }
//
//        // 删除已合并的 block
//        blocks_.erase(id);
//    }
//
//    // 更新 ModelActor
//    // 更新目标 block 的 patchIDs
//    // 调用 ModelActor 的 merge_blocks 函数更新 Actor
//    actor_->merge_blocks(block_ids, block_ids[0], target_block->patchIDs);
//    actor_->update_group(target_block->groupID, groups_[target_block->groupID]->blockIDs);
//    for (int modified_group : modified_groups) {
//        if (groups_.count(modified_group)) {
//            actor_->update_group(modified_group, groups_[modified_group]->blockIDs);
//        } else {
//            actor_->update_group(modified_group, {});
//        }
//    }
//}
//
//
//
//
//void Model::merge_groups(const std::vector<int>& group_ids) {
//    if (group_ids.empty()) {
//        throw std::invalid_argument("group_ids cannot be empty.");
//    }
//
//    /* 验证 group_ids 是否有效
//    for (int id : group_ids) {
//        if (groups_.find(id) == groups_.end()) {
//            throw std::runtime_error("Group ID not found: " + std::to_string(id));
//        }
//    }*/
//
//    // 获取目标 group（第一个 group）
//    int target_group_id = group_ids[0];
//    auto& target_group = groups_[target_group_id];
//
//    // 合并其他 group 的内容到目标 group
//    for (size_t i = 1; i < group_ids.size(); ++i) {
//        int id = group_ids[i];
//        auto& group_to_merge = groups_[id];
//
//        // 合并 blockIDs
//        for (int block_id : group_to_merge->blockIDs) {
//            target_group->blockIDs.insert(block_id);
//            blocks_[block_id]->groupID = target_group_id;
//        }
//
//        // 删除已合并的 group
//        groups_.erase(id);
//    }
//
//    // 更新 ModelActor
//    //actor_->update_group(target_group_id, target_group->blockIDs); // 假设 ModelActor 有更新 group 的方法
//    actor_->merge_groups(group_ids, group_ids[0], target_group->blockIDs);
//}
//
//
//
//
//
//void Model::remesh_block(const std::vector<int>& block_ids) {
//    // 验证 block_id 是否有效
//    std::unordered_set<int> patch_ids_set;
//    for (int block_id : block_ids) {
//        if (blocks_.find(block_id) == blocks_.end()) {
//            throw std::runtime_error("Block ID not found in group: " + std::to_string(block_id));
//        }
//        auto& block = blocks_[block_id];
//        patch_ids_set.insert(block->patchIDs.begin(), block->patchIDs.end());
//    }
//
//    // 将 patch_ids_set 转换为 vector
//    std::vector<int> patch_ids(patch_ids_set.begin(), patch_ids_set.end());
//
//    // 调用 ModelUtil::remesh_patches 方法对 patch 重新网格化
//    mesh_ = ModelUtil::remesh_patches(std::move(mesh_), patch_ids);
//
//    // 更新所有相关的 patches
//    update_patches(patch_ids);
//
//    // 更新所有相关的 actors
//    update_actors(patch_ids);
//}
//
//void Model::remesh_group(const std::vector<int>& group_ids) {
//    // 收集所有 patch_ids
//    std::unordered_set<int> patch_ids_set;
//    for (int group_id : group_ids)
//    {
//        // 验证 group_id 是否有效
//        if (groups_.find(group_id) == groups_.end()) {
//            throw std::runtime_error("Group ID not found: " + std::to_string(group_id));
//        }
//
//        // 获取指定 group
//        auto& group = groups_[group_id];
//
//        for (int block_id : group->blockIDs) {
//            if (blocks_.find(block_id) == blocks_.end()) {
//                throw std::runtime_error("Block ID not found in group: " + std::to_string(block_id));
//            }
//            auto& block = blocks_[block_id];
//            patch_ids_set.insert(block->patchIDs.begin(), block->patchIDs.end());
//        }
//    }
//
//    // 将 patch_ids_set 转换为 vector
//    std::vector<int> patch_ids(patch_ids_set.begin(), patch_ids_set.end());
//
//    // 调用 ModelUtil::remesh_patches 方法对这些 patch 进行重新网格化
//    mesh_ = ModelUtil::remesh_patches(std::move(mesh_), patch_ids);
//
//    // 更新所有相关的 patches
//    update_patches(patch_ids);
//
//    // 更新所有相关的 actors
//    update_actors(patch_ids);
//}
//
//
//int Model::face_patch_id(int face_id) {
//    // 遍历所有 patches
//    for (const auto& [patch_id, patch_ptr] : patches_) {
//        if (std::find(patch_ptr->faceIDs_.begin(), patch_ptr->faceIDs_.end(), face_id) != patch_ptr->faceIDs_.end()) {
//            return patch_id; // 找到对应的 patch_id
//        }
//    }
//
//    // 如果找不到 face_id，抛出异常或返回特殊值
//    throw std::runtime_error("Face ID not found in any patch.");
//}
//
//const std::vector<int>& Model::patch_face_ids(int patch_id) {
//    // 检查 patch_id 是否存在
//    if (patches_.find(patch_id) == patches_.end()) {
//        throw std::runtime_error("Patch ID not found: " + std::to_string(patch_id));
//    }
//
//    // 返回对应 patch 的 faceIDs_
//    return patches_[patch_id]->faceIDs_;
//}
//
//const std::vector<int>& Model::patch_vertex_ids(int patch_id) {
//    // 检查 patch_id 是否存在
//    if (patches_.find(patch_id) == patches_.end()) {
//        throw std::runtime_error("Patch ID not found: " + std::to_string(patch_id));
//    }
//
//    // 返回对应 patch 的 vertexIDs_
//    return patches_[patch_id]->vertexIDs_;
//}
//
//int Model::patch_block_id(int patch_id) {
//    // 遍历 blocks_ 查找包含 patch_id 的 block
//    for (const auto& [block_id, block_ptr] : blocks_) {
//        if (block_ptr->patchIDs.find(patch_id) != block_ptr->patchIDs.end()) {
//            return block_id; // 找到对应的 block_id
//        }
//    }
//
//    // 如果找不到 patch_id，抛出异常
//    throw std::runtime_error("Patch ID not found in any block.");
//}
//
//int Model::block_group_id(int patch_id) {
//    // 先获取 patch 对应的 block_id
//    int block_id = patch_block_id(patch_id);
//
//    // 遍历 groups_ 查找包含 block_id 的 group
//    for (const auto& [group_id, group_ptr] : groups_) {
//        if (group_ptr->blockIDs.find(block_id) != group_ptr->blockIDs.end()) {
//            return group_id; // 找到对应的 group_id
//        }
//    }
//
//    // 如果找不到 block_id，抛出异常
//    throw std::runtime_error("Block ID not found in any group.");
//}
//
//ModelActor& Model::actor() {
//    // 检查 actor_ 是否有效
//    if (!actor_) {
//        throw std::runtime_error("ModelActor is not initialized.");
//    }
//
//    // 返回 actor 的引用
//    return *actor_;
//}
//void Model::update_actors(const std::vector<int>& patch_ids)
//{
//    std::unordered_set<int> block_ids, group_ids;
//    for (int patch_id : patch_ids) {
//        block_ids.insert(patch_block_id(patch_id));
//        actor_->update_patch(patch_id, patches_[patch_id]->vertexPoints_, patches_[patch_id]->faceTriangles_);
//    }
//    for (int block_id : block_ids) {
//        group_ids.insert(block_group_id(block_id));
//        actor_->update_block(block_id, blocks_[block_id]->patchIDs);
//    }
//    for (int group_id : group_ids)
//    {
//        actor_->update_group(group_id, groups_[group_id]->blockIDs);
//    }
//}
//
//
//// 优化 update_patches 的实现，减少网格遍历次数
//
//void Model::update_patches(const std::vector<int>& patch_ids) {
//    // 使用 unordered_set 来处理 patch_ids 的快速查找
//    std::unordered_set<int> patch_id_set(patch_ids.begin(), patch_ids.end());
//
//    // 调用重载函数
//    update_patches(patch_id_set);
//}
//
//void Model::update_patches(const std::unordered_set<int>& patch_ids) {
//    // 删除指定的 Patch 数据
//    for (int patch_id : patch_ids) {
//        patches_.erase(patch_id);
//    }
//
//    // 分组面片：按 Patch ID 将面片分组
//    std::unordered_map<int, std::vector<MeshLib::CTMesh::CFace*>> patch_faces;
//    for (auto& face : mesh_->faces()) {
//        int face_patch_id = face->get_g();
//        if (patch_ids.find(face_patch_id) != patch_ids.end()) {
//            patch_faces[face_patch_id].push_back(face);
//        }
//    }
//
//    // 遍历每个 Patch 的面片组
//    for (const auto& [patch_id, faces] : patch_faces) {
//        // 初始化 Patch
//        auto& patch = patches_[patch_id];
//        if (!patch) {
//            patch = std::make_unique<Patch>();
//            patch->id_ = patch_id;
//        }
//
//        // 追踪 Patch 内部的顶点
//        std::unordered_map<int, int> vertex_id_map;  // 全局顶点 ID 到 Patch 内局部索引的映射
//
//        // 遍历面片，更新 Patch 的顶点和三角形信息
//        for (auto* face : faces) {
//            // 添加当前面的三角形
//            std::array<int, 3> triangle{};
//            int i = 0;
//
//            // 遍历面的顶点
//            for (MeshLib::CTMesh::FaceVertexIterator vi(face); !vi.end(); ++vi) {
//                auto vertex = *vi;
//
//                // 如果顶点未被记录，添加到 Patch 的顶点列表
//                if (vertex_id_map.find(vertex->id()) == vertex_id_map.end()) {
//                    vertex_id_map[vertex->id()] = patch->vertexIDs_.size();
//                    patch->vertexIDs_.push_back(vertex->id());
//
//                    // 插入顶点坐标
//                    CPoint& vp = vertex->point();
//                    patch->vertexPoints_.emplace_back(std::array<double, 3>{vp[0], vp[1], vp[2]});
//                }
//
//                // 设置三角形索引
//                triangle[i++] = vertex_id_map[vertex->id()];
//            }
//
//            // 添加三角形到 Patch 的索引列表
//            patch->faceTriangles_.push_back(triangle);
//            patch->faceIDs_.push_back(face->id());
//        }
//    }
//}
///*void Model::update_patches(const std::unordered_set<int>& patch_ids) {
//    // 直接使用给定的 patch_ids 集合进行查找
//    for (int patch_id : patch_ids)
//    {
//        patches_.erase(patch_id);
//    }
//
//    // 遍历 mesh 中的面并更新对应的 patch
//    for (auto& face : mesh_->faces()) {
//        int face_patch_id = face->get_g();
//        if (patch_ids.find(face_patch_id) != patch_ids.end()) {
//            auto& patch = patches_[face_patch_id];
//            if (!patch) {
//                patch = std::make_unique<Patch>();
//                patch->id_ = face_patch_id;
//            }
//            patch->faceIDs_.push_back(face->id());
//
//            patch->faceTriangles_.emplace_back();
//            // 更新顶点信息
//            int i = 0;
//           for (MeshLib::CTMesh::FaceVertexIterator vi(face); !vi.end(); vi++) {
//                auto vertex = *vi;
//                patch->faceTriangles_.back()[i++] = patch->vertexIDs_.size();
//                patch->vertexIDs_.push_back(vertex->id());
//                CPoint& vp = vertex->point();
//                patch->vertexPoints_.emplace_back(std::array { vp[0], vp[1], vp[2] });
//            }
//        }
//    }
//}*/
