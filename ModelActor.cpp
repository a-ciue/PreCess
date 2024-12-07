#include "ModelActor.h"
#include "Model.h"
#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNamedColors.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

#include "ModelUtil.h"
#include "Style.h"
#include <vtkAppendPolyData.h>

// ModelActor::ModelActor(Model* model)
//     : model_(model)
//{
//     model_->update_patches();
// }

// void ModelActor::set_model(Model* model)
//{
//     model_ = model;
// }

void ModelActor::bind_renderer(vtkRenderer* renderer, RenderMode mode)
{
    ActorMap* mode_actors {};
    switch (mode) {
    case RenderMode::Face: {
        face_renderer_ = renderer;
        mode_actors = &patch_actors_;
        break;
    }
    case RenderMode::Block: {
        block_renderer_ = renderer;
        mode_actors = &block_actors_;
        break;
    }
    case RenderMode::Group: {
        group_renderer_ = renderer;
        mode_actors = &group_actors_;
        break;
    }
    }

    for (auto&& [_, actor] : *mode_actors) {
        renderer->AddActor(actor);
    }
}

int ModelActor::patch_actor_id(vtkActor* actor)
{
    if (patch_actor_id_.count(actor)) {
        return patch_actor_id_[actor];
    }
    throw std::runtime_error("patch actor not valid");
}

int ModelActor::block_actor_id(vtkActor* actor)
{
    if (block_actor_id_.count(actor)) {
        return block_actor_id_[actor];
    }
    throw std::runtime_error("block actor not valid");
}

int ModelActor::group_actor_id(vtkActor* actor)
{
    if (group_actor_id_.count(actor)) {
        return group_actor_id_[actor];
    }
    throw std::runtime_error("group actor not valid");
}

ModelActor::ModelActor(const std::unordered_map<int, std::unique_ptr<Patch>>& patches,
    const std::unordered_map<int, std::unique_ptr<Block>>& blocks,
    const std::unordered_map<int, std::unique_ptr<Group>>& groups)
{
    for (auto&& [_, patch] : patches) {
        update_patch(patch->id_, patch->vertexPoints_, patch->faceTriangles_);
    }

    for (auto&& [patch_id, patch_actor] : patch_actors_) {
        patch_actor_id_[patch_actor] = patch_id;
    }

    for (auto&& [_, block] : blocks) {
        update_block(block->id, block->patchIDs);
    }

    for (auto&& [block_id, block_actor] : block_actors_) {
        block_actor_id_[block_actor] = block_id;
    }

    for (auto&& [_, group] : groups) {
        update_group(group->id, group->blockIDs);
    }

    for (auto&& [group_id, group_actor] : group_actors_) {
        group_actor_id_[group_actor] = group_id;
    }
}

ModelActor::~ModelActor()
{
    if (face_renderer_) {
        // ActorMap& face_actors = *mode_actors_[RenderMode::Face];
        for (auto&& [_, actor] : patch_actors_) {
            face_renderer_->RemoveActor(actor);
        }
    }
    if (block_renderer_) {
        // ActorMap& block_actors = *mode_actors_[RenderMode::Block];
        for (auto&& [_, actor] : block_actors_) {
            block_renderer_->RemoveActor(actor);
        }
    }
    if (group_renderer_) {
        // ActorMap& group_actors = *mode_actors_[RenderMode::Group];
        for (auto&& [_, actor] : group_actors_) {
            group_renderer_->RemoveActor(actor);
        }
    }
}

void ModelActor::merge_blocks(const std::vector<int>& block_ids, int father_block, const std::unordered_set<int>& father_block_patches)
{
    assert(block_actors_.count(father_block));

    for (int erase_id : block_ids) {
        if (erase_id != father_block) {
            vtkActor* erase_actor = block_actors_[erase_id];
            // 删除被合并block actor
            if (block_renderer_) {
                block_renderer_->RemoveActor(erase_actor);
            }
            block_actors_.erase(erase_id);
            block_actor_id_.erase(erase_actor);
        }
    }

    update_block(father_block, father_block_patches);
}

void ModelActor::merge_groups(const std::vector<int>& group_ids, int father_group, const std::unordered_set<int>& father_group_blocks)
{
    assert(group_actors_.find(father_group) != group_actors_.end());

    for (int erase_id : group_ids) {
        if (erase_id != father_group) {
            // 删除被合并block actor
            vtkActor* erase_actor = group_actors_[erase_id];
            if (group_renderer_) {
                group_renderer_->RemoveActor(erase_actor);
            }
            group_actors_.erase(erase_id);
            group_actor_id_.erase(erase_actor);
        }
    }

    update_group(father_group, father_group_blocks);
}

void ModelActor::update_patch(int patch_id, const std::vector<std::array<double, 3>>& points, const std::vector<std::array<int, 3>>& triangles)
{
	if (!patch_actors_.count(patch_id))
	{
        patch_actors_[patch_id] = vtkSmartPointer<vtkActor>::New();
	}
    vtkSmartPointer<vtkActor> patch_actor = patch_actors_[patch_id];

    // vtkPolyData
    vtkSmartPointer<vtkPoints> points_data = vtkSmartPointer<vtkPoints>::New();
    for (const std::array<double, 3>& point : points) {
        points_data->InsertNextPoint(point.data());
    }

    vtkSmartPointer<vtkCellArray> triangles_data = vtkSmartPointer<vtkCellArray>::New();
    for (const std::array<int, 3>& triangle : triangles) {
        vtkIdType triangle_idxs[3] { triangle[0], triangle[1], triangle[2] };
        triangles_data->InsertNextCell(3, triangle_idxs);
    }

    vtkNew<vtkPolyData> vtkData;
    vtkData->SetPoints(points_data);
    vtkData->SetPolys(triangles_data);

    // Mapper & Actor
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(vtkData);
    patch_actor->SetMapper(mapper);

    double r = ModelUtil::randomSequence->GetNextRangeValue(0.1, 0.6),
           g = ModelUtil::randomSequence->GetNextRangeValue(0.1, 0.6),
           b = ModelUtil::randomSequence->GetNextRangeValue(0.1, 0.6);

    patch_actor->GetProperty()->SetDiffuseColor(r, g, b);
    patch_actor->GetProperty()->SetDiffuse(0.8);
    patch_actor->GetProperty()->SetSpecular(0.5);
    patch_actor->GetProperty()->SetSpecularColor(
        ModelUtil::colors->GetColor3d("White").GetData());
    patch_actor->GetProperty()->SetSpecularPower(30.0);
}

void ModelActor::update_block(int block_id, const std::unordered_set<int>& block_patches)
{
	if (!block_actors_.count(block_id))
	{
        block_actors_[block_id] = vtkSmartPointer<vtkActor>::New();
	}

    std::vector<vtkActor*> patch_actors;
    patch_actors.reserve(block_patches.size());
    for (int patch_id : block_patches) {
        assert(patch_actors_.count(patch_id));
        patch_actors.push_back(patch_actors_[patch_id]);
    }

    _merge_actors(block_actors_[block_id], patch_actors);
}

void ModelActor::update_group(int group_id, const std::unordered_set<int>& group_blocks)
{
	if (!group_actors_.count(group_id))
	{
        group_actors_[group_id] = vtkSmartPointer<vtkActor>::New();
	}
	if (group_blocks.empty())
	{
        group_renderer_->RemoveActor(group_actors_[group_id]);
        group_actor_id_.erase(group_actors_[group_id]);
        group_actors_.erase(group_id);
        return;
	}

    std::vector<vtkActor*> block_actors;
    block_actors.reserve(group_blocks.size());
    for (int block_id : group_blocks) {
        assert(patch_actors_.count(block_id));
        block_actors.push_back(block_actors_[block_id]);
    }

    _merge_actors(group_actors_[group_id], block_actors);
}

void ModelActor::_merge_actors(vtkActor* father_actor, const std::vector<vtkActor*>& actors)
{
    // Append PolyData
    vtkNew<vtkAppendPolyData> append_data;
    for (vtkActor* actor : actors) {
        vtkPolyDataMapper* mapper = vtkPolyDataMapper::SafeDownCast(actor->GetMapper());
        vtkPolyData* data = mapper->GetInput();

        append_data->AddInputData(data);
    }
    append_data->Update();

    // Mapper & Actor
    vtkNew<vtkPolyDataMapper> father_mapper;
    father_mapper->SetInputConnection(append_data->GetOutputPort());
    father_actor->SetMapper(father_mapper);

    double r = ModelUtil::randomSequence->GetNextRangeValue(0.1, 0.6),
           g = ModelUtil::randomSequence->GetNextRangeValue(0.1, 0.6),
           b = ModelUtil::randomSequence->GetNextRangeValue(0.1, 0.6);

    father_actor->GetProperty()->SetDiffuseColor(r, g, b);
    father_actor->GetProperty()->SetDiffuse(0.8);
    father_actor->GetProperty()->SetSpecular(0.5);
    father_actor->GetProperty()->SetSpecularColor(
        ModelUtil::colors->GetColor3d("White").GetData());
    father_actor->GetProperty()->SetSpecularPower(30.0);
}

// old
// void ModelActor::update_block(int block_id)
//{
//    vtkActor* block_actor = block_actors_[block_id];
//    const std::vector<int>& patch_ids = model_->block_patch_ids(block_id);
//
//    // Append PolyData
//    vtkNew<vtkAppendPolyData> append_data;
//    for (int patch_id : patch_ids) {
//        vtkActor* patch_actor = patch_actors_[patch_id];
//        vtkPolyDataMapper* patch_mapper = vtkPolyDataMapper::SafeDownCast(patch_actor->GetMapper());
//        vtkPolyData* patch_data = patch_mapper->GetInput();
//
//        append_data->AddInputData(patch_data);
//    }
//    append_data->Update();
//
//    // Mapper & Actor
//    vtkNew<vtkPolyDataMapper> block_mapper;
//    block_mapper->SetInputConnection(append_data->GetOutputPort());
//    block_actor->SetMapper(block_mapper);
//
//    double r = ModelUtil::randomSequence.GetNextRangeValue(0.1, 0.6),
//           g = ModelUtil::randomSequence.GetNextRangeValue(0.1, 0.6),
//           b = ModelUtil::randomSequence.GetNextRangeValue(0.1, 0.6);
//
//    block_actor->GetProperty()->SetDiffuseColor(r, g, b);
//    block_actor->GetProperty()->SetDiffuse(0.8);
//    block_actor->GetProperty()->SetSpecular(0.5);
//    block_actor->GetProperty()->SetSpecularColor(
//        ModelUtil::colors.GetColor3d("White").GetData());
//    block_actor->GetProperty()->SetSpecularPower(30.0);
//
//    // ？
//    if (block_renderer_) {
//    }
//}
//
// old
// void ModelActor::update_group(int group_id)
//{
//    vtkActor* group_actor = group_actors_[group_id];
//    const std::vector<int>& block_ids = model_->group_block_ids(group_id);
//
//    // Append PolyDataOutput
//    vtkNew<vtkAppendPolyData> append_data;
//    for (int block_id : block_ids) {
//        vtkActor* block_actor = patch_actors_[block_id];
//        vtkPolyDataMapper* block_mapper = vtkPolyDataMapper::SafeDownCast(block_actor->GetMapper());
//        vtkAlgorithmOutput* block_data = block_mapper->GetInputConnection(0,0);
//
//        append_data->AddInputConnection(block_data);
//    }
//    append_data->Update();
//
//    // Mapper & Actor
//    vtkNew<vtkPolyDataMapper> group_mapper;
//    group_mapper->SetInputConnection(append_data->GetOutputPort());
//    group_actor->SetMapper(group_mapper);
//
//    double r = ModelUtil::randomSequence.GetNextRangeValue(0.1, 0.6),
//           g = ModelUtil::randomSequence.GetNextRangeValue(0.1, 0.6),
//           b = ModelUtil::randomSequence.GetNextRangeValue(0.1, 0.6);
//
//    group_actor->GetProperty()->SetDiffuseColor(r, g, b);
//    group_actor->GetProperty()->SetDiffuse(0.8);
//    group_actor->GetProperty()->SetSpecular(0.5);
//    group_actor->GetProperty()->SetSpecularColor(
//        ModelUtil::colors.GetColor3d("White").GetData());
//    group_actor->GetProperty()->SetSpecularPower(30.0);
//
//    // ？
//    if (group_renderer_) {
//    }
//
//}
//
