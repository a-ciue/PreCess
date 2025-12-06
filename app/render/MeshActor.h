#ifndef MESH_ACTOR_H
#define MESH_ACTOR_H
#include "Core.h"
#include <vtkPropCollection.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNamedColors.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkActor.h>
class vtkGeometryFilter;
class vtkExtractGeometry;
class vtkExtractPolyDataGeometry;
class vtkPoints;
class vtkUnstructuredGrid;
class vtkRenderer;
class MeshActorSelectOp;

//! @brief 负责管理Model的Actor
class MeshActor {
    friend MeshActorSelectOp;

public:
    static vtkNew<vtkMinimalStandardRandomSequence> randomSequence;
    static vtkNew<vtkNamedColors> colors;

    MeshActor(vtkRenderer* renderer, bool is_edge_render = true, bool is_vertex_render = true, ModelRenderMode render_mode = ModelRenderMode::Face);
    ~MeshActor();

    void loadModelData(const MeshDataVtk& model_data);

    void setVisibility(bool visibility);
    /**
     * @brief 设置或取消裁剪平面
     * @param plane 裁剪平面，传入nullptr则取消裁剪
     */
    void setClipPlane(vtkPlane* plane);
    void setRenderEdge(bool is_render);
    void setRenderVertex(bool is_render);
    void setRenderMode(ModelRenderMode render_mode);

    bool getIsEdgeRender();
    bool getIsVertexRender();
   
    ModelRenderMode getMeshRenderMode();
    /**
     * @brief 渲染的属性所在actor位置
     */
     enum ElementType {
        VERTEX,
        EDGE,
        FACE,
        SOLID
    };
     /**
     * @brief 属性渲染方式
      */
    enum Mode {
        RGB,
        SCALAR,
        UV,
        VECTOR
    };
    void setActiveScalarAttribute(std::string attr_name, ElementType type);
    void setActiveVectorAttribute(std::string attr_name, ElementType type);
     /**
      * @brief 设置属性渲染方式
      * @param mode 渲染方式 0:RGB 1:SCALAR 2:UV 3:VECTOR
      * @param type 属性类型 0:VERTEX 1:EDGE 2:FACE 3:SOLID
      * @param texturePath 贴图路径
      */
    void setAttriMode(std::string attr_name, Mode mode, ElementType type, std::string texturePath);
    /**
     * @brief 取消属性渲染
     */
    void cancelActiveAttribute();
   

private:
    void cancelActiveGlyph3D();
    void setTextureImage(std::string texturePath);

    ModelRenderMode render_mode_;
    bool edge_render_{ true };
    bool vertex_render_ {};
    bool visibility_{ true };
    std::unique_ptr<MeshDataVtk> model_data_;

    vtkPlane* clip_plane_ {};

    vtkNew<vtkExtractPolyDataGeometry> vertex_clipper_;
    vtkNew<vtkExtractPolyDataGeometry> edge_clipper_;
    vtkNew<vtkExtractPolyDataGeometry> face_clipper_;
    vtkNew<vtkExtractGeometry> solid_clipper_;

    vtkNew<vtkGeometryFilter> solid_filter_;

    vtkNew<vtkPolyDataMapper> vertex_mapper_;
    vtkNew<vtkPolyDataMapper> edge_mapper_;
    vtkNew<vtkPolyDataMapper> face_mapper_;
    vtkNew<vtkPolyDataMapper> solid_mapper_;

    vtkNew<vtkActor> solid_actor_;
    vtkNew<vtkActor> face_actor_;
    vtkNew<vtkActor> edge_actor_;
    vtkNew<vtkActor> vertex_actor_;

    vtkNew<vtkUnstructuredGrid> solid_data_;
    vtkNew<vtkPolyData> face_data_;
    vtkNew<vtkPolyData> edge_data_;
    vtkNew<vtkPolyData> vertex_data_;

    vtkRenderer* renderer_;

    vtkNew<vtkActor> actor_;
    //Face mapper
    vtkNew<vtkCompositePolyDataMapper> block_mapper_;
    void createBlockMapper(const MeshDataVtk& model_data);
    static void _createSolidUGird(const MeshDataVtk& model_data, vtkPoints& points, vtkUnstructuredGrid& solid_data);
};
#endif // MODEL_ACTOR_H
