#ifndef Model_OPERATOR_H
#define Model_OPERATOR_H
#include "ModelOperator.h"

void ModelOperator::write_mesh(const std::filesystem::path& mesh_path, ModelRenderMode mode, const QString& extension)
{
    model_->write_mesh(mesh_path, mode, extension);
}

void ModelOperator::split_face(QSelection* selection)
{
    model_->split_face(selection);
    observer_->notifyModelChanged(model_->getId());
}

void ModelOperator::split_edge(QSelection* selection)
{
    model_->split_edge(selection);
    observer_->notifyModelChanged(model_->getId());
}

void ModelOperator::merge_blocks(QSelection* selection)
{
    model_->merge_blocks(selection);
    observer_->notifyModelChanged(model_->getId());
}

void ModelOperator::merge_groups(QSelection* selection)
{
    model_->merge_groups(selection);
    observer_->notifyModelChanged(model_->getId());
}

void ModelOperator::remesh_block(QSelection* selection)
{
    model_->remesh_block(selection);
    observer_->notifyModelChanged(model_->getId());
}

void ModelOperator::remesh_group(QSelection* selection)
{
    model_->remesh_group(selection);
    observer_->notifyModelChanged(model_->getId());
}


#endif // Model_OPERATOR_H
