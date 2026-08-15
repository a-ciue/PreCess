/**
 * @file TopologyDiagnosticActor.h
 * @brief 网格拓扑诊断结果的独立叠加渲染对象
 */
#pragma once

#include "Core.h"
#include "TopologyDiagnosticCategory.h"

#include <array>
#include <vtkActor.h>
#include <vtkExtractPolyDataGeometry.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>

class vtkRenderer;
class vtkPlane;

/**
 * @brief 把拓扑诊断集合渲染为独立的边、点和面 Actor
 */
class TopologyDiagnosticActor {
public:
    explicit TopologyDiagnosticActor(vtkRenderer* renderer);
    ~TopologyDiagnosticActor();

    /** @brief 根据最新网格和诊断结果重建叠加数据 */
    void loadModelData(const MeshDataVtk& model_data);
    /** @brief 设置某一诊断类别是否显示 */
    void setCategoryVisible(TopologyDiagnosticCategory category, bool visible);
    /** @brief 设置主网格是否可见，诊断层跟随组件可见性 */
    void setMeshVisible(bool visible);
    /** @brief 设置与主网格一致的裁剪平面，传入 nullptr 时取消裁剪 */
    void setClipPlane(vtkPlane* plane);
    /** @brief 设置二面角边筛选范围，单位为度 */
    void setDihedralAngleRange(double minimum, double maximum);

private:
    static constexpr std::size_t category_count_ = kTopologyDiagnosticCategoryCount;

    void rebuildDihedralEdges();
    void applyVisibility();

    vtkRenderer* renderer_ {};
    bool mesh_visible_ { true };
    double dihedral_minimum_ { 30.0 };
    double dihedral_maximum_ { 180.0 };
    std::array<bool, category_count_> category_visible_ {};
    std::array<vtkNew<vtkPolyData>, category_count_> data_;
    std::array<vtkNew<vtkExtractPolyDataGeometry>, category_count_> clippers_;
    std::array<vtkNew<vtkPolyDataMapper>, category_count_> mappers_;
    std::array<vtkNew<vtkActor>, category_count_> actors_;
    vtkNew<vtkPoints> points_; //> 当前组件诊断几何共用的点坐标
    std::shared_ptr<const MeshTopologyDiagnosticResult> diagnostics_; //> 二面角范围变化时重新筛选的只读结果
};
