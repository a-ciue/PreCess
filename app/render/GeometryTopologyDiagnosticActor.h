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
#include <atomic>
#include <cstdint>
#include <memory>

class GeometryTopologyDiagnosticResult;
struct GeometryTopologyDiagnosticOptions;
struct GeometryTopologyDiagnosticOutcome;
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

    /**
     * @brief 应用后台算完的诊断结果并刷新渲染；必须由渲染线程调用。
     * @return 本次是否应用了新结果（调用方据此决定是否需要重绘）。
     */
    bool pumpCompletedTasks();

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
    /** @brief 汇总所有"已启用但尚未算完"的类别，生成一次后台请求的参数；无待算类别时返回 false。 */
    bool pendingOptions(GeometryTopologyDiagnosticOptions& options) const;
    /** @brief 提交一次后台诊断请求；已有在途请求时直接返回。 */
    void submitDiagnostics();
    /** @brief 作废在途请求（形状或阈值变化时调用），旧结果回传后会被丢弃。 */
    void invalidateTasks();
    /** @brief 把后台结果并入缓存并标记对应类别已完成。 */
    void mergeOutcome(GeometryTopologyDiagnosticOutcome& outcome);
    /** @brief 只把本次请求真正覆盖到的类别标记为失败，不误伤计算期间新启用的类别。 */
    void markFailedCategories(const GeometryTopologyDiagnosticOptions& options);
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
    //! 已提交后台计算、结果未回传的类别。
    std::array<bool, kGeometryTopologyDiagnosticCategoryCount> category_pending_ {};
    //! 后台计算失败的类别；形状或阈值变化后清除，允许重试。
    std::array<bool, kGeometryTopologyDiagnosticCategoryCount> category_failed_ {};
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
    //! 组件自身的世代号；每次作废在途任务时递增，用于丢弃过期结果。
    std::uint64_t task_generation_ { 0 };
    //! 当前在途请求的代号；为 0 表示没有在途请求。
    std::uint64_t outstanding_generation_ { 0 };
    //! 当前在途请求的协作式取消标记；作废或析构时置位，让后台尽快退出。
    std::shared_ptr<std::atomic<bool>> outstanding_cancel_;
};
