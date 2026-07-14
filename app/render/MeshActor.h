#ifndef MESH_ACTOR_H
#define MESH_ACTOR_H
#include "renderStrategy/AttributeCommon.h"
#include "Core.h"
#include "renderStrategy/IAttributeRenderStrategy.h"
#include <optional>
#include <vtkActor.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNamedColors.h>
#include <vtkPropCollection.h>
#include <vtkPoints.h>
#include <vtkIdTypeArray.h>
class vtkGeometryFilter;
class vtkExtractGeometry;
class vtkExtractPolyDataGeometry;
class vtkPoints;
class vtkPolyData;
class vtkUnstructuredGrid;
class vtkRenderer;
class vtkScalarBarActor;
class MeshActorSelectOp;
class AttributeOperator;

// 保存面属性渲染临时修改 face_mapper_ 前的相对偏移参数
struct FaceAttributeOffsetState {
    bool active { false };
    double factor { 0.0 };
    double units { 0.0 };
};

//! @brief 负责管理Model的Actor
class MeshActor {
    friend MeshActorSelectOp;
    friend AttributeOperator;

public:
    static vtkNew<vtkMinimalStandardRandomSequence> randomSequence;
    static vtkNew<vtkNamedColors> colors;

    MeshActor(
        vtkRenderer* renderer,
        vtkPoints* global_points,
        bool is_edge_render = true,
        ModelRenderMode render_mode = ModelRenderMode::Face,
        vtkScalarBarActor* scalar_bar = nullptr);
    ~MeshActor();

    void loadModelData(const MeshDataVtk& model_data);

    void setVisibility(bool visibility);
    /**
     * @brief 设置或取消裁剪平面
     * @param plane 裁剪平面，传入nullptr则取消裁剪
     */
    void setClipPlane(vtkPlane* plane);
    void setRenderEdge(bool is_render);
    void setRenderMode(ModelRenderMode render_mode);

    bool getIsEdgeRender();

    ModelRenderMode getMeshRenderMode();
    /**
     * @brief 取消属性渲染
     */
    void cancelActiveAttribute();
    /**
     * @brief 设置渲染策略
     * @param strategy 渲染策略
     */
    void setRenderStrategy(std::unique_ptr<IAttributeRenderStrategy> strategy);
    /**
     * @brief 渲染属性
     * @param attr_name 属性名称
     * @param args 渲染参数
     */
    void renderAttribute(
        const std::string& attr_name,
        std::map<std::string, std::any> args);

    void ensureOriginalPointIds();

private:
    std::unique_ptr<IAttributeRenderStrategy> render_strategy_;
    ModelRenderMode render_mode_;
    bool edge_render_ { true };
    bool visibility_ { true };
    std::unique_ptr<MeshDataVtk> model_data_;

    vtkPlane* clip_plane_ {};
    vtkNew<vtkExtractPolyDataGeometry> edge_clipper_;
    vtkNew<vtkExtractPolyDataGeometry> face_clipper_;
    vtkNew<vtkExtractGeometry> solid_clipper_;

    vtkNew<vtkGeometryFilter> solid_filter_;

    vtkNew<vtkPolyDataMapper> edge_mapper_;
    vtkNew<vtkPolyDataMapper> face_mapper_;
    vtkNew<vtkPolyDataMapper> solid_mapper_;
    vtkNew<vtkPolyDataMapper> glyph3D_mapper_;

    vtkNew<vtkActor> solid_actor_;
    vtkNew<vtkActor> face_actor_;
    vtkNew<vtkActor> edge_actor_;
    vtkNew<vtkActor> glyph3D_actor_;

    vtkNew<vtkUnstructuredGrid> solid_data_;
    vtkNew<vtkPolyData> face_data_;
    vtkNew<vtkPolyData> edge_data_;

    // 缓存面单元中心点。
    vtkNew<vtkPolyData> face_cell_centers_;
    // 缓存体单元中心点。
    vtkNew<vtkPolyData> solid_cell_centers_;

    FaceAttributeOffsetState face_attribute_offset_;

    vtkRenderer* renderer_;

    // 指向渲染窗口共享的标量颜色表，不负责其生命周期。
    vtkScalarBarActor* scalar_bar_ {};

    vtkNew<vtkActor> actor_;

    vtkPoints* global_points_ {};
    vtkNew<vtkIdTypeArray> original_point_ids_;

    vtkNew<vtkCompositePolyDataMapper> block_mapper_;
    void createBlockMapper(const MeshDataVtk& model_data);
    static void _createSolidUGird(const MeshDataVtk& model_data, vtkPoints& points, vtkUnstructuredGrid& solid_data);
};
#endif // MODEL_ACTOR_H
