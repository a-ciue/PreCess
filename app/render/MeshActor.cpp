#include "MeshActor.h"
#include "Core.h"
#include "TopologyDiagnosticActor.h"
#include "renderStrategy/AttributeOperator.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <vtkActor.h>
#include <vtkCellData.h>
#include <vtkDataSetAttributes.h>
#include <vtkMinimalStandardRandomSequence.h>
#include <vtkNamedColors.h>
#include <vtkPlane.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPropAssembly.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkTrivialProducer.h>
#include <vtkUnstructuredGrid.h>
vtkNew<vtkMinimalStandardRandomSequence> MeshActor::randomSequence;
vtkNew<vtkNamedColors> MeshActor::colors;

namespace {
/** @brief 为渲染端建立独立数据集包装，底层只读数组由快照保活。 */
vtkSmartPointer<vtkPolyData> renderPolyData(vtkPolyData* input)
{
    auto result = vtkSmartPointer<vtkPolyData>::New();
    result->ShallowCopy(input);
    return result;
}

/** @brief 按数组名称同步活跃标量与 UV，保持过滤后的点/单元对应关系。 */
void syncAttributes(vtkDataSetAttributes& source, vtkDataSetAttributes& output)
{
    auto* scalars = source.GetScalars();
    output.SetActiveScalars(scalars ? scalars->GetName() : nullptr);
    auto* tcoords = source.GetTCoords();
    output.SetTCoords(tcoords ? output.GetArray(tcoords->GetName()) : nullptr);
}
}

MeshActor::MeshActor(vtkRenderer* renderer)
    : renderer_(renderer)
{
    if (!renderer_) {
        throw std::invalid_argument("MeshActor: renderer cannot be null");
    }
    vtkNew<vtkNamedColors> colors;

    this->renderer_->AddActor(this->solid_actor_);
    this->renderer_->AddActor(this->face_actor_);
    this->renderer_->AddActor(this->edge_actor_);
    this->renderer_->AddActor(this->glyph3D_actor_);

    this->edge_actor_->GetProperty()->SetLineWidth(2);

    this->solid_actor_->SetMapper(solid_mapper_);
    this->face_actor_->SetMapper(face_mapper_);
    this->edge_actor_->SetMapper(edge_mapper_);
    this->glyph3D_actor_->SetMapper(glyph3D_mapper_);
    topology_diagnostics_ = std::make_unique<TopologyDiagnosticActor>(renderer_);

    for (auto* source : { face_source_.GetPointer(), edge_source_.GetPointer(),
             solid_surface_source_.GetPointer(), solid_internal_source_.GetPointer() }) {
        vtkNew<vtkPolyData> empty;
        source->SetOutput(empty);
    }
    face_mapper_->SetInputConnection(face_source_->GetOutputPort());
    edge_mapper_->SetInputConnection(edge_source_->GetOutputPort());
    solid_mapper_->SetInputConnection(solid_surface_source_->GetOutputPort());
}

MeshActor::~MeshActor()
{
    if (this->renderer_) {
        renderer_->RemoveActor(this->solid_actor_);
        renderer_->RemoveActor(this->face_actor_);
        renderer_->RemoveActor(this->edge_actor_);
        renderer_->RemoveActor(this->glyph3D_actor_);
    }
}

void MeshActor::applyDataSnapshot(std::shared_ptr<const MeshDataSnapshot> snapshot)
{
    if (!snapshot || snapshot->generation < data_generation_
        || (snapshot->generation == data_generation_ && snapshot->revision <= data_revision_))
        return;
    const bool reloaded = snapshot->generation != data_generation_;
    const bool display_changed = visibility_ != snapshot->visible;
    data_snapshot_ = std::move(snapshot);
    const auto& data = *data_snapshot_;
    data_generation_ = data.generation;
    data_revision_ = data.revision;
    visibility_ = data.visible;
    if (reloaded) {
        // 属性和选择继续读取完整原始数据，但包装独立于 GUI 管线。
        model_data_ = data.model_data;
        face_data_->ShallowCopy(data.original_faces);
        edge_data_->ShallowCopy(data.original_edges);
        solid_data_->ShallowCopy(data.original_solids);
        points_->SetData(solid_data_->GetPoints()->GetData());
        // vtkPoints 的边界等缓存也归本线程，不能共享 GUI 点对象的可变缓存。
        face_data_->SetPoints(points_);
        edge_data_->SetPoints(points_);
        solid_data_->SetPoints(points_);
        edge_cell_centers_ = nullptr;
        face_cell_centers_ = nullptr;
        solid_cell_centers_ = nullptr;
        edge_mapper_->ScalarVisibilityOff();
        face_mapper_->ScalarVisibilityOff();
        solid_mapper_->ScalarVisibilityOff();
        topology_diagnostics_->loadModelData(*model_data_);
    }
    visible_faces_ = renderPolyData(data.visible_faces);
    visible_edges_ = renderPolyData(data.visible_edges);
    solid_surface_ = renderPolyData(data.solid_surface);
    solid_internal_edges_ = renderPolyData(data.solid_internal_edges);
    face_source_->SetOutput(visible_faces_);
    edge_source_->SetOutput(visible_edges_);
    solid_surface_source_->SetOutput(solid_surface_);
    solid_internal_source_->SetOutput(solid_internal_edges_);
    clip_plane_->SetOrigin(data.clip.origin.data());
    clip_plane_->SetNormal(data.clip.normal.data());
    topology_diagnostics_->setClipPlane(data.clip.enabled ? clip_plane_.GetPointer() : nullptr);
    syncDisplayAttributes();
    // 发布新的连通性后刷新绘制缓存，不能因连接未改变而复用旧线框着色器。
    face_mapper_->Modified();
    if (reloaded || display_changed)
        applyStyle();
}

void MeshActor::syncDisplayAttributes()
{
    auto sync_data = [](vtkDataSet& source, vtkPolyData& output) {
        syncAttributes(*source.GetPointData(), *output.GetPointData());
        syncAttributes(*source.GetCellData(), *output.GetCellData());
    };
    if (visible_faces_)
        sync_data(*face_data_, *visible_faces_);
    if (visible_edges_)
        sync_data(*edge_data_, *visible_edges_);
    if (solid_surface_)
        sync_data(*solid_data_, *solid_surface_);
    if (solid_internal_edges_)
        sync_data(*solid_data_, *solid_internal_edges_);
}

void MeshActor::setVisibility(bool visibility)
{
    this->visibility_ = visibility;
    applyStyle();
}

bool MeshActor::isVisible() const
{
    return visibility_ && style_ != MeshRenderStyle::Hidden;
}

void MeshActor::setRenderStyle(MeshRenderStyle style)
{
    this->style_ = style;
    applyStyle();
}

MeshRenderStyle MeshActor::getRenderStyle() const
{
    return style_;
}

void MeshActor::applyStyle()
{
    topology_diagnostics_->setMeshVisible(visibility_ && style_ != MeshRenderStyle::Hidden);
    if (style_ == MeshRenderStyle::Hidden || !visibility_) {
        solid_actor_->SetVisibility(false);
        face_actor_->SetVisibility(false);
        edge_actor_->SetVisibility(false);
        glyph3D_actor_->SetVisibility(false);
        return;
    }

    glyph3D_actor_->SetVisibility(true);

    if (style_ == MeshRenderStyle::WireframeInternal) {
        solid_mapper_->SetInputConnection(solid_internal_source_->GetOutputPort());
    } else {
        solid_mapper_->SetInputConnection(solid_surface_source_->GetOutputPort());
    }

    switch (style_) {
    case MeshRenderStyle::FaceWithEdges:
        solid_actor_->SetVisibility(true);
        solid_actor_->GetProperty()->SetRepresentationToSurface();
        solid_actor_->GetProperty()->SetEdgeVisibility(true);
        solid_actor_->GetProperty()->SetOpacity(1.0);
        face_actor_->SetVisibility(true);
        face_actor_->GetProperty()->SetRepresentationToSurface();
        face_actor_->GetProperty()->SetEdgeVisibility(true);
        face_actor_->GetProperty()->SetOpacity(1.0);
        edge_actor_->SetVisibility(this->visibility_);
        break;
    case MeshRenderStyle::Face:
        solid_actor_->SetVisibility(true);
        solid_actor_->GetProperty()->SetRepresentationToSurface();
        solid_actor_->GetProperty()->SetEdgeVisibility(false);
        solid_actor_->GetProperty()->SetOpacity(1.0);
        face_actor_->SetVisibility(true);
        face_actor_->GetProperty()->SetRepresentationToSurface();
        face_actor_->GetProperty()->SetEdgeVisibility(false);
        face_actor_->GetProperty()->SetOpacity(1.0);
        edge_actor_->SetVisibility(false);
        break;
    case MeshRenderStyle::Transparent75:
        solid_actor_->SetVisibility(true);
        solid_actor_->GetProperty()->SetRepresentationToSurface();
        solid_actor_->GetProperty()->SetEdgeVisibility(true);
        solid_actor_->GetProperty()->SetOpacity(0.75);
        face_actor_->SetVisibility(true);
        face_actor_->GetProperty()->SetRepresentationToSurface();
        face_actor_->GetProperty()->SetEdgeVisibility(true);
        face_actor_->GetProperty()->SetOpacity(0.75);
        edge_actor_->SetVisibility(this->visibility_);
        break;
    case MeshRenderStyle::Transparent50:
        solid_actor_->SetVisibility(true);
        solid_actor_->GetProperty()->SetRepresentationToSurface();
        solid_actor_->GetProperty()->SetEdgeVisibility(true);
        solid_actor_->GetProperty()->SetOpacity(0.50);
        face_actor_->SetVisibility(true);
        face_actor_->GetProperty()->SetRepresentationToSurface();
        face_actor_->GetProperty()->SetEdgeVisibility(true);
        face_actor_->GetProperty()->SetOpacity(0.50);
        edge_actor_->SetVisibility(this->visibility_);
        break;
    case MeshRenderStyle::Transparent25:
        solid_actor_->SetVisibility(true);
        solid_actor_->GetProperty()->SetRepresentationToSurface();
        solid_actor_->GetProperty()->SetEdgeVisibility(true);
        solid_actor_->GetProperty()->SetOpacity(0.25);
        face_actor_->SetVisibility(true);
        face_actor_->GetProperty()->SetRepresentationToSurface();
        face_actor_->GetProperty()->SetEdgeVisibility(true);
        face_actor_->GetProperty()->SetOpacity(0.25);
        edge_actor_->SetVisibility(this->visibility_);
        break;
    case MeshRenderStyle::WireframeInternal:
        solid_actor_->SetVisibility(true);
        solid_actor_->GetProperty()->SetRepresentationToWireframe();
        solid_actor_->GetProperty()->SetOpacity(1.0);
        face_actor_->SetVisibility(true);
        face_actor_->GetProperty()->SetRepresentationToWireframe();
        face_actor_->GetProperty()->SetOpacity(1.0);
        edge_actor_->SetVisibility(this->visibility_);
        break;
    case MeshRenderStyle::WireframeSurface:
        solid_actor_->SetVisibility(true);
        solid_actor_->GetProperty()->SetRepresentationToWireframe();
        solid_actor_->GetProperty()->SetOpacity(1.0);
        face_actor_->SetVisibility(true);
        face_actor_->GetProperty()->SetRepresentationToWireframe();
        face_actor_->GetProperty()->SetOpacity(1.0);
        edge_actor_->SetVisibility(false);
        break;
    default:
        break;
    }
}

TopologyDiagnosticActor& MeshActor::topologyDiagnostics()
{
    return *topology_diagnostics_;
}

void MeshActor::cancelActiveAttribute()
{
    if (render_strategy_) {
        AttributeOperator op(this);
        render_strategy_->cancelActiveAttribute(op);
        syncDisplayAttributes();
    }
}

void MeshActor::setRenderStrategy(std::unique_ptr<IAttributeRenderStrategy> strategy)
{
    render_strategy_ = std::move(strategy);
}

void MeshActor::renderAttribute(
    const std::string& attr_name,
    std::map<std::string, std::any> args)
{
    if (render_strategy_) {
        AttributeOperator op(this);
        render_strategy_->render(op, attr_name, args);
        syncDisplayAttributes();
    }
}
