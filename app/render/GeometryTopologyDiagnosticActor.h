/**
 * @file GeometryTopologyDiagnosticActor.h
 * @brief 定义 OCC 几何拓扑诊断结果的独立叠加渲染对象。
 */
#pragma once

#include "GeometryTopologyDiagnosticCategory.h"

#include <IVtkOCC_Shape.hxx>
#include <IVtkTools_SubPolyDataFilter.hxx>
#include <Standard_Handle.hxx>
#include <vtkActor.h>
#include <vtkNew.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>

#include <array>
#include <memory>

class GeometryTopologyDiagnosticResult;
class TopoDS_Shape;
class vtkDataArray;
class vtkPolyData;
class vtkRenderer;

typedef Handle(IVtkOCC_Shape) GeometryDiagnosticOccShapeHandle;

/**
 * @brief 计算并显示一个 GeometryActor 对应的 OCC 几何拓扑诊断结果。
 */
class GeometryTopologyDiagnosticActor {
public:
    explicit GeometryTopologyDiagnosticActor(vtkRenderer* renderer);
    ~GeometryTopologyDiagnosticActor();

    /** @brief 装载新的 OCC 形状及其已经生成的 VTK 线、面数据。 */
    void loadShape(const TopoDS_Shape& shape,
        const GeometryDiagnosticOccShapeHandle& occ_shape,
        vtkPolyData* line_data,
        vtkDataArray* line_sub_ids,
        vtkPolyData* face_data,
        vtkDataArray* face_sub_ids);

    /** @brief 启用或停用一种几何拓扑诊断类别。 */
    void setCategoryEnabled(GeometryTopologyDiagnosticCategory category, bool enabled);
    /** @brief 设置所属几何组件是否可见。 */
    void setGeometryVisible(bool visible);
    /** @brief 设置细小边诊断使用的长度阈值。 */
    void setSmallEdgeLengthThreshold(double threshold);
    /** @brief 设置细小面诊断使用的面积阈值。 */
    void setSmallFaceAreaThreshold(double threshold);

private:
    /** @brief 保存一种诊断类别的子形状过滤器、映射器和 Actor。 */
    struct DiagnosticPipeline {
        vtkNew<IVtkTools_SubPolyDataFilter> filter;
        vtkNew<vtkPolyDataMapper> mapper;
        vtkNew<vtkActor> actor;
    };

    /** @brief 使用固定屏幕点大小标出肉眼难以看到的细小几何。 */
    struct SizeMarkerPipeline {
        vtkNew<vtkPolyData> data;
        vtkNew<vtkPolyDataMapper> mapper;
        vtkNew<vtkActor> actor;
    };

    /** @brief 首次需要显示诊断类别时按类别依赖计算并缓存结果。 */
    void ensureDiagnostics(GeometryTopologyDiagnosticCategory category);
    /** @brief 按缓存结果更新指定类别的子形状过滤集合。 */
    void rebuildCategory(GeometryTopologyDiagnosticCategory category);
    /** @brief 更新细小边中点或细小面质心的固定屏幕尺寸标记。 */
    void rebuildSizeMarker(GeometryTopologyDiagnosticCategory category);
    /** @brief 同步所有诊断 Actor 的最终可见性。 */
    void applyVisibility();

    vtkRenderer* renderer_ {};
    bool geometry_visible_ { true };
    double small_edge_length_threshold_ { 1.0e-6 };
    double small_face_area_threshold_ { 1.0e-12 };
    std::array<bool, kGeometryTopologyDiagnosticCategoryCount> category_enabled_ {};
    std::array<bool, kGeometryTopologyDiagnosticCategoryCount> category_computed_ {};
    std::array<DiagnosticPipeline, kGeometryTopologyDiagnosticCategoryCount> pipelines_;
    DiagnosticPipeline invalid_edge_pipeline_;
    SizeMarkerPipeline small_edge_marker_;
    SizeMarkerPipeline small_face_marker_;

    TopoDS_Shape* shape_ {};
    std::unique_ptr<TopoDS_Shape> owned_shape_;
    GeometryDiagnosticOccShapeHandle occ_shape_;
    vtkPolyData* line_data_ {};
    vtkPolyData* face_data_ {};
    vtkDataArray* line_sub_ids_ {};
    vtkDataArray* face_sub_ids_ {};
    std::unique_ptr<GeometryTopologyDiagnosticResult> diagnostics_;
};
