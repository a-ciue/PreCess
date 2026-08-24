#ifndef MESH_ACTOR_H
#define MESH_ACTOR_H
#include "renderStrategy/AttributeCommon.h"
#include "Core.h"
#include "renderStrategy/IAttributeRenderStrategy.h"
#include <optional>
#include <unordered_map>
#include <vtkActor.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkExtractEdges.h>
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

    explicit MeshActor(vtkRenderer* renderer);
    ~MeshActor();

    void loadModelData(const MeshDataVtk& model_data);

    void setVisibility(bool visibility);
    bool isVisible() const;
    /**
     * @brief 设置或取消裁剪平面
     * @param plane 裁剪平面，传入nullptr则取消裁剪
     */
    void setClipPlane(vtkPlane* plane);
    void setRenderStyle(MeshRenderStyle style);
    MeshRenderStyle getRenderStyle() const;

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

private:
    void applyStyle();

    std::unique_ptr<IAttributeRenderStrategy> render_strategy_;
    MeshRenderStyle style_ { MeshRenderStyle::FaceWithEdges };
    bool visibility_ { true };
    std::unique_ptr<MeshDataVtk> model_data_;

    vtkPlane* clip_plane_ {};
    vtkNew<vtkExtractPolyDataGeometry> edge_clipper_;
    vtkNew<vtkExtractPolyDataGeometry> face_clipper_;
    vtkNew<vtkExtractGeometry> solid_clipper_;

    vtkNew<vtkGeometryFilter> solid_filter_;
    vtkNew<vtkExtractEdges> solid_edge_extractor_;

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

    // 缓存边单元中心点，供边向量 glyph 复用。
    vtkNew<vtkPolyData> edge_cell_centers_;
    // 缓存面单元中心点。
    vtkNew<vtkPolyData> face_cell_centers_;
    // 缓存体单元中心点。
    vtkNew<vtkPolyData> solid_cell_centers_;

    FaceAttributeOffsetState face_attribute_offset_;

    vtkRenderer* renderer_;

    // 组件私有点集：坐标随 loadModelData 从 MeshData::vertex_positions_ 同步，
    // 连通性数组直接以组件内局部点 id 作 VTK 点索引。
    vtkNew<vtkPoints> points_;
    //> 拾取标签 vtkOriginalPointIds：存局部点 id，高亮提取直接作 VTK 点索引；
    //> 与提取过滤器自动生成的同名数组同语义（均为局部点下标），无需担心覆盖。
    //> 全局点 id 不随数据集散发，跨层身份在出口（get()/PickInfo）经 id 查询桥统一换算。
    vtkNew<vtkIdTypeArray> original_point_ids_;

    static void _createSolidUGird(const MeshDataVtk& model_data, vtkPoints& points, vtkUnstructuredGrid& solid_data);
};
#endif // MODEL_ACTOR_H
