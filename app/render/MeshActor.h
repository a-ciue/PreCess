#ifndef MESH_ACTOR_H
#define MESH_ACTOR_H
#include "Core.h"
#include "MeshPipelineData.h"
#include "renderStrategy/AttributeCommon.h"
#include "renderStrategy/IAttributeRenderStrategy.h"
#include <optional>
#include <unordered_map>
#include <vector>
#include <vtkActor.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkIdTypeArray.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNamedColors.h>
#include <vtkPoints.h>
#include <vtkPropCollection.h>
#include <vtkSmartPointer.h>
class vtkTrivialProducer;
class vtkPlane;
class vtkPoints;
class vtkPolyData;
class vtkUnstructuredGrid;
class vtkRenderer;
class MeshActorSelectOp;
class AttributeOperator;
class TopologyDiagnosticActor;

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

    /** @brief 安装 GUI 线程已完成的只读数据快照，拒绝旧代次和旧版本。 */
    void applyDataSnapshot(std::shared_ptr<const MeshDataSnapshot> snapshot);
    /** @brief 返回当前源数据代次，供选择消息校验原始面身份。 */
    std::uint64_t dataGeneration() const { return data_generation_; }

    void setVisibility(bool visibility);
    bool isVisible() const;
    /** @brief 查询网格显隐开关，供组件级反转使用，不受渲染样式影响。 */
    bool isVisibilityEnabled() const { return visibility_; }
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

    /** @brief 获取本组件独立的拓扑诊断渲染对象 */
    TopologyDiagnosticActor& topologyDiagnostics();

private:
    void applyStyle();
    /** @brief 将原始数据的活跃属性选择同步到本线程的可见数据包装。 */
    void syncDisplayAttributes();

    std::unique_ptr<IAttributeRenderStrategy> render_strategy_;
    MeshRenderStyle style_ { MeshRenderStyle::FaceWithEdges };
    bool visibility_ { true };
    bool faces_only_ { false }; //> 面隔离期间遮住不经过面过滤管线的渲染对象
    std::shared_ptr<const MeshDataVtk> model_data_;
    std::shared_ptr<const MeshDataSnapshot> data_snapshot_;
    std::uint64_t data_generation_ { };
    std::uint64_t data_revision_ { };
    std::unique_ptr<TopologyDiagnosticActor> topology_diagnostics_;

    // 发布结果只经过渲染端的固定 Producer，不连接 GUI Filter 的输出端口。
    vtkNew<vtkTrivialProducer> face_source_;
    vtkNew<vtkTrivialProducer> edge_source_;
    vtkNew<vtkTrivialProducer> solid_surface_source_;
    vtkNew<vtkTrivialProducer> solid_internal_source_;
    vtkNew<vtkPlane> clip_plane_;
    vtkSmartPointer<vtkPolyData> visible_faces_;
    vtkSmartPointer<vtkPolyData> visible_edges_;
    vtkSmartPointer<vtkPolyData> solid_surface_;
    vtkSmartPointer<vtkPolyData> solid_internal_edges_;

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

    // 边、面、体单元中心点在首次向量渲染时计算，网格重新加载后置空失效。
    vtkSmartPointer<vtkPolyData> edge_cell_centers_;
    vtkSmartPointer<vtkPolyData> face_cell_centers_;
    vtkSmartPointer<vtkPolyData> solid_cell_centers_;

    FaceAttributeOffsetState face_attribute_offset_;

    vtkRenderer* renderer_;

    // 渲染端私有点包装，底层只读坐标由已发布快照保活。
    vtkNew<vtkPoints> points_;
};
#endif // MODEL_ACTOR_H
