#ifndef Model_OPERATOR_H
#define Model_OPERATOR_H
#include "ModelOperator.h"

void ModelOperator::write_mesh(const std::filesystem::path& mesh_path, RenderMode mode, const QString& extension)
{
    m_model->write_mesh(mesh_path, mode, extension);
}

void ModelOperator::split_face(QSelection* selection)
{
    m_model->split_face(selection);
}

void ModelOperator::split_edge(QSelection* selection)
{
    m_model->split_edge(selection);
}

void ModelOperator::merge_blocks(QSelection* selection)
{
    m_model->merge_blocks(selection);
}

void ModelOperator::merge_groups(QSelection* selection)
{
    m_model->merge_groups(selection);
}

void ModelOperator::remesh_block(QSelection* selection)
{
    m_model->remesh_block(selection);
}

void ModelOperator::remesh_group(QSelection* selection)
{
    m_model->remesh_group(selection);
}


#endif // Model_OPERATOR_H
