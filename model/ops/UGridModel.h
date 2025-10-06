#pragma once
#include "MeshModelBase.h"
#include "Core.h"

#include <unordered_set>
#include <vtkSmartPointer.h>

class vtkUnstructuredGrid;
    /**
 * @brief 模型层辅助数据结构 UGrid
 */
class UGridModel : public MeshModelBase {
public:
    UGridModel(vtkUnstructuredGrid& mesh);
    ~UGridModel() override;

    /**
     * @brief 更新MeshData
     * @param mesh_data 待更新的核心网格数据结构W
     */
    void update(MeshData& mesh_data) override;

    /**
     * @brief 从指定的 MeshData 对象更新当前对象的数据。
     * @param mesh_data 用于更新的 MeshData 对象。
     */
    void updateFrom(const MeshData& mesh_data) override;

private:
    vtkSmartPointer<vtkUnstructuredGrid> mesh_;
};
