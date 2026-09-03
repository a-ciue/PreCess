/**
 * @file TopologyDiagnosticActor.h
 * @brief 网格拓扑诊断结果的独立叠加渲染对象
 */
#pragma once

#include "Core.h"
#include "TopologyDiagnosticCategory.h"

#include <array>
#include <memory>
#include <vtkActor.h>
#include <vtkExtractPolyDataGeometry.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>

class vtkRenderer;
class vtkPlane;
struct MeshTopologyDiagnosticResult;

/**
 * @brief 把拓扑诊断集合渲染为独立的边、点和面 Actor
 */
class TopologyDiagnosticActor {
public:
    explicit TopologyDiagnosticActor(vtkRenderer* renderer);
    ~TopologyDiagnosticActor();

    /** @brief 保存最新网格数据，并在类别已启用时重建诊断叠加数据 */
    void loadModelData(const MeshDataVtk& model_data);
    /** @brief 设置某一诊断类别是否启用 */
    void setCategoryEnabled(TopologyDiagnosticCategory category, bool enabled);
    /** @brief 设置主网格是否可见，诊断层跟随组件可见性 */
    void setMeshVisible(bool visible);
    /** @brief 设置与主网格一致的裁剪平面，传入 nullptr 时取消裁剪 */
    void setClipPlane(vtkPlane* plane);
    /** @brief 设置二面角边筛选范围，单位为度 */
    void setDihedralAngleRange(double minimum, double maximum);

private:
    static constexpr std::size_t category_count_ = kTopologyDiagnosticCategoryCount;

    /** @brief 保存一种图元的诊断数据、裁剪器、映射器和 Actor */
    struct DiagnosticPipeline {
        vtkNew<vtkPolyData> data;
        vtkNew<vtkExtractPolyDataGeometry> clipper;
        vtkNew<vtkPolyDataMapper> mapper;
        vtkNew<vtkActor> actor;
    };

    /** @brief 首次需要显示诊断类别时计算并缓存结果 */
    void ensureDiagnostics();
    /** @brief 当前是否至少启用了一个诊断类别 */
    bool hasEnabledCategory() const;
    /** @brief 按当前边界面开关重建面诊断数据 */
    void rebuildFaceData();
    /** @brief 按当前类别开关重建点诊断数据 */
    void rebuildPointData();
    /** @brief 按当前类别开关和角度范围重建边诊断数据 */
    void rebuildEdgeData();
    void applyVisibility();

    vtkRenderer* renderer_ {};
    bool mesh_visible_ { true };
    double dihedral_minimum_ { 0.0 };
    double dihedral_maximum_ { 150.0 };
    std::array<bool, category_count_> category_enabled_ {};
    DiagnosticPipeline point_pipeline_;
    DiagnosticPipeline edge_pipeline_;
    DiagnosticPipeline face_pipeline_;
    vtkNew<vtkPoints> points_; //> 当前组件诊断几何共用的点坐标
    const MeshDataVtk* model_data_ {}; //> 指向所属 MeshActor 持有的当前渲染数据视图
    std::unique_ptr<MeshTopologyDiagnosticResult> diagnostics_; //> 当前网格的渲染层懒计算结果
};
