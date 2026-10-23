/**
 * @file TestMeshActorWithData.h
 * @brief 在测试所属线程准备网格数据，并向渲染对象发布计算结果。
 */
#ifndef TEST_MESH_ACTOR_WITH_DATA_H
#define TEST_MESH_ACTOR_WITH_DATA_H

#include "MeshActor.h"
#include "MeshPipelineData.h"

/** @brief 将既有测试的显隐操作转发到数据管线，保留对 MeshActor 的行为断言。 */
class TestMeshActorWithData : public MeshActor {
public:
    /** @brief 绑定渲染器；数据管线与测试对象在同一线程创建和更新。 */
    explicit TestMeshActorWithData(vtkRenderer* renderer)
        : MeshActor(renderer)
    {
    }

    /** @brief 加载基础网格，并发布清空旧隐藏状态后的渲染结果。 */
    void loadModelData(const MeshDataVtk& model_data)
    {
        data_.loadModelData(model_data);
        publishData();
    }

    /** @brief 同步数据管线的整体显隐，供后续反转和指定面恢复使用。 */
    void setVisibility(bool visibility)
    {
        data_.setVisibility(visibility);
        publishData();
    }

    /** @brief 按原始面 ID 累积隐藏，并安装更新后的可见数据。 */
    void hideFaces(const std::vector<vtkIdType>& face_ids)
    {
        data_.hideFaces(face_ids);
        publishData();
    }

    /** @brief 恢复指定原始面，同时保留其他面的显隐。 */
    void showFaces(const std::vector<vtkIdType>& face_ids)
    {
        data_.showFaces(face_ids);
        publishData();
    }

    /** @brief 隔离指定原始面，并将隔离状态与数据一同发布。 */
    void isolateFaces(const std::vector<vtkIdType>& face_ids)
    {
        data_.isolateFaces(face_ids);
        publishData();
    }

    /** @brief 清空手动隐藏和隔离状态，并发布完整可见面集。 */
    void showAllFaces()
    {
        data_.showAllFaces();
        publishData();
    }

    /** @brief 反转手动显隐，并返回更新后的整体显隐开关。 */
    bool reverseVisibility()
    {
        const bool visibility = data_.reverseVisibility();
        publishData();
        return visibility;
    }

    /** @brief 在测试所属线程更新整单元裁剪，并发布其输出。 */
    void setClipPlane(vtkPlane* plane)
    {
        data_.setClipPlane(plane);
        publishData();
    }

private:
    /** @brief 显式发布管线输出，渲染对象仅安装数据而不连接计算过滤器。 */
    void publishData()
    {
        applyDataSnapshot(data_.createSnapshot());
    }

    MeshPipelineData data_;
};

#endif // TEST_MESH_ACTOR_WITH_DATA_H
