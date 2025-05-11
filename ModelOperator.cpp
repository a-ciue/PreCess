#ifndef Model_OPERATOR_H
#define Model_OPERATOR_H
#include "ModelOperator.h"

void ModelOperator::write_mesh(const std::filesystem::path& mesh_path, RenderMode mode, const QString& extension)
{
    model_->write_mesh(mesh_path, mode, extension);
}

void ModelOperator::split_face(QSelection* selection)
{
    model_->split_face(selection);
    observer_->notifyModelChanged(model_->getModelName());
}

void ModelOperator::split_edge(QSelection* selection)
{
    model_->split_edge(selection);
}

void ModelOperator::merge_blocks(QSelection* selection)
{
    model_->merge_blocks(selection);
}

void ModelOperator::merge_groups(QSelection* selection)
{
    model_->merge_groups(selection);
}

void ModelOperator::remesh_block(QSelection* selection)
{
    model_->remesh_block(selection);
}

void ModelOperator::remesh_group(QSelection* selection)
{
    model_->remesh_group(selection);
}


#endif // Model_OPERATOR_H
