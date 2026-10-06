/**
 * @file GeometryTopologyDiagnosticActor.h
 * @brief 定义 OCC 几何拓扑诊断结果的独立叠加渲染对象。
 */
#pragma once

#include "Core.h"
#include "GeometryTopologyDiagnosticCategory.h"

#include <IVtkOCC_Shape.hxx>
#include <Standard_Handle.hxx>
#include <vtkActor.h>
#include <vtkNew.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>

#include <array>
#include <string>
#include <memory>

class GeometryTopologyDiagnosticResult;
struct GeometryTopologyDiagnosticOptions;
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
    /** @brief 仅同步全部类别开关，不触发诊断重建；供加载新 Shape 前配置。 */
    void setCategoryFlagsOnly(
        const std::array<bool, kGeometryTopologyDiagnosticCategoryCount>& enabled);
    /** @brief 设置所属几何组件是否可见。 */
    void setGeometryVisible(bool visible);
    /** @brief 设置细小边诊断使用的长度阈值。 */
    void setSmallEdgeLengthThreshold(double threshold);
    /** @brief 设置细小面诊断使用的面积阈值。 */
    void setSmallFaceAreaThreshold(double threshold);
    /** @brief 设置本 Actor 所属组件的显示标签（形如 "2 (Wing)"），只用于日志标注归属。 */
    void setComponentLabel(std::string label) { component_label_ = std::move(label); }

private:
    /** @brief 保存一种诊断类别的独立静态数据、映射器和 Actor。 */
    struct DiagnosticPipeline {
        vtkNew<vtkPolyData> data;
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
    /** @brief 把本次算出的**已启用类别**的计数与具体子形状索引写入日志。
     *  @param options 本次实际启用的诊断类别；未启用的类别不写日志，避免以 0 混入。 */
    void logDiagnosticDetails(const GeometryTopologyDiagnosticResult& result,
        const GeometryTopologyDiagnosticOptions& options, double elapsed_ms);
    /** @brief 结果已算过时复用缓存，但仍按统一格式输出一次该类别日志。 */
    void logCachedDiagnostics(GeometryTopologyDiagnosticCategory category);
    /** @brief 按缓存结果重建指定类别的独立诊断数据。 */
    void rebuildCategory(GeometryTopologyDiagnosticCategory category);
    /** @brief 更新细小边中点或细小面质心的固定屏幕尺寸标记。 */
    void rebuildSizeMarker(GeometryTopologyDiagnosticCategory category);
    /** @brief 同步所有诊断 Actor 的最终可见性。 */
    void applyVisibility();

    vtkRenderer* renderer_ {};
    bool geometry_visible_ { true };
    double small_edge_length_threshold_ { 0.01 };
    double small_face_area_threshold_ { 0.01 };
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
    //! 所属组件的显示标签（id + 名称），仅用于日志标注结果归属；空表示尚未设置。
    std::string component_label_;
};
