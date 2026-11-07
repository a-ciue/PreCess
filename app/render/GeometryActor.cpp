#include "GeometryActor.h"
#include "Core.h"
#include "GeometryTopologyDiagnosticActor.h"
#include "SurfacePrecision.h"
#include <BRepTools.hxx>
#include <IVtkTools_ShapeDataSource.hxx>
#include <IVtkVTK_ShapeData.hxx>
#include <Prs3d_Drawer.hxx>
#include <TopoDS_Shape.hxx>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkIdList.h>
#include <vtkIdTypeArray.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

#include <cmath>
#include <utility>

GeometryActor::GeometryActor(vtkRenderer* renderer)
{
    this->renderer_ = renderer;
    this->visibility_ = true;
    line_only_ = vtkSmartPointer<vtkPolyData>::New();
    poly_only_ = vtkSmartPointer<vtkPolyData>::New();
    topology_diagnostics_ = std::make_unique<GeometryTopologyDiagnosticActor>(renderer_);
}

GeometryActor::~GeometryActor()
{
    deleteGeometryActor();
}

void GeometryActor::loadShape(const GeometryDataVtk& geometry_data)
{
    // 直接沿用模型形状及索引，显示重建只更新可再生的离散缓存。
    occ_shape_ = new IVtkOCC_Shape(geometry_data.shape);
    occ_shape_->SetId(static_cast<IVtk_IdType>(geometry_data.component_id));
    geometry_index_ = geometry_data.geometry_index;
    rebuildMesh(surface_precision::kDefault);
    topology_diagnostics_->loadShape(occ_shape_->GetShape(), occ_shape_,
        line_only_, poly_only_);
    createActors();
}

void GeometryActor::setSurfacePrecision(int precision)
{
    rebuildMesh(precision);
    topology_diagnostics_->refreshDisplay();
}

void GeometryActor::rebuildMesh(int precision)
{
    // OCCT 通常复用已有的更细三角化；先清缓存保证细到粗切档也生效。
    BRepTools::Clean(occ_shape_->GetShape());
    const auto& settings = surface_precision::kLevels[static_cast<size_t>(precision)];
    occ_shape_->Attributes()->SetDeviationCoefficient(settings.deviation_coefficient);
    occ_shape_->Attributes()->SetDeviationAngle(settings.angle_degrees * std::acos(-1.0) / 180.0);
    vtkNew<IVtkTools_ShapeDataSource> source;
    source->SetShape(occ_shape_);
    source->Update();
    vtkPolyData* src = source->GetOutput();
    const vtkIdType nV = src->GetNumberOfVerts();
    const vtkIdType nL = src->GetNumberOfLines();
    const vtkIdType nP = src->GetNumberOfPolys();

    auto* subshape_ids = static_cast<vtkIdTypeArray*>(
        src->GetCellData()->GetArray(IVtkVTK_ShapeData::ARRNAME_SUBSHAPE_IDS()));

    // 沿用 OCCT 的点、线、面区段；点线区段只保留真正归属拓扑点、边的单元。
    vtkNew<vtkIdList> line_cells;
    vtkNew<vtkIdList> face_cells;
    line_cells->Allocate(nV + nL);
    face_cells->Allocate(nP);
    for (vtkIdType cell_id = 0; cell_id < nV + nL; ++cell_id) {
        const auto owner_type = occ_shape_->GetSubShape(subshape_ids->GetValue(cell_id)).ShapeType();
        if (owner_type == TopAbs_VERTEX || owner_type == TopAbs_EDGE)
            line_cells->InsertNextId(cell_id);
    }
    for (vtkIdType cell_id = nV + nL; cell_id < nV + nL + nP; ++cell_id)
        face_cells->InsertNextId(cell_id);
    line_only_->Initialize();
    poly_only_->Initialize();
    line_only_->AllocateExact(nV, nV, nL, src->GetLines()->GetNumberOfConnectivityIds(),
        0, 0, 0, 0);
    poly_only_->AllocateExact(0, 0, 0, 0, nP,
        src->GetPolys()->GetNumberOfConnectivityIds(), 0, 0);
    for (const auto& [data, cells] : { std::pair { line_only_.GetPointer(), line_cells.GetPointer() },
             std::pair { poly_only_.GetPointer(), face_cells.GetPointer() } }) {
        // 一次复制所需点和单元，保留法线属性与身份数组，排除无关点对边界的影响。
        data->GetPointData()->CopyNormalsOn();
        data->GetPointData()->CopyAllocate(src->GetPointData());
        data->GetCellData()->CopyAllocate(src->GetCellData(), cells->GetNumberOfIds());
        data->CopyCells(src, cells);
    }
}

void GeometryActor::createActors()
{
    vtkNew<vtkPolyDataMapper> poly_mapper;
    poly_mapper->SetInputData(poly_only_);
    poly_mapper->SetRelativeCoincidentTopologyPolygonOffsetParameters(0.0, 1.0);

    poly_actor_->SetMapper(poly_mapper);
    poly_actor_->GetProperty()->SetColor(200.0 / 255.0, 200.0 / 255.0, 200.0 / 255.0);
    renderer_->AddActor(poly_actor_);

    vtkNew<vtkPolyDataMapper> line_mapper;
    line_mapper->SetInputData(line_only_);
    line_mapper->SetRelativeCoincidentTopologyLineOffsetParameters(0, 4);
    line_mapper->SetRelativeCoincidentTopologyPointOffsetParameter(8);

    line_actor_->SetMapper(line_mapper);
    line_actor_->GetProperty()->LightingOff();
    line_actor_->GetProperty()->SetLineWidth(2.0);
    line_actor_->GetProperty()->RenderLinesAsTubesOn();
    line_actor_->GetProperty()->SetPointSize(6.0);
    line_actor_->GetProperty()->SetColor(0.0, 0.0, 0.0);
    renderer_->AddActor(line_actor_);
    applyStyle();
    topology_diagnostics_->setGeometryVisible(isVisible());
}

void GeometryActor::deleteGeometryActor()
{
    if (this->renderer_) {
        renderer_->RemoveActor(this->poly_actor_);
        renderer_->RemoveActor(this->line_actor_);
    }
}

void GeometryActor::setVisibility(bool visibility)
{
    this->visibility_ = visibility;
    applyStyle();
    topology_diagnostics_->setGeometryVisible(isVisible());
}

bool GeometryActor::isVisible() const
{
    return visibility_ && style_ != GeometryRenderStyle::Hidden;
}

void GeometryActor::setRenderStyle(GeometryRenderStyle style)
{
    this->style_ = style;
    applyStyle();
    topology_diagnostics_->setGeometryVisible(isVisible());
}

GeometryRenderStyle GeometryActor::getRenderStyle() const
{
    return style_;
}

GeometryTopologyDiagnosticActor& GeometryActor::topologyDiagnostics()
{
    return *topology_diagnostics_;
}

void GeometryActor::applyStyle()
{
    if (style_ == GeometryRenderStyle::Hidden || !visibility_) {
        poly_actor_->SetVisibility(false);
        line_actor_->SetVisibility(false);
        return;
    }

    switch (style_) {
    case GeometryRenderStyle::SurfaceWithEdges:
        poly_actor_->SetVisibility(true);
        poly_actor_->GetProperty()->SetRepresentationToSurface();
        poly_actor_->GetProperty()->SetEdgeVisibility(false);
        poly_actor_->GetProperty()->SetOpacity(1.0);
        line_actor_->SetVisibility(true);
        break;
    case GeometryRenderStyle::Surface:
        poly_actor_->SetVisibility(true);
        poly_actor_->GetProperty()->SetRepresentationToSurface();
        poly_actor_->GetProperty()->SetEdgeVisibility(false);
        poly_actor_->GetProperty()->SetOpacity(1.0);
        line_actor_->SetVisibility(false);
        break;
    case GeometryRenderStyle::Transparent75:
        poly_actor_->SetVisibility(true);
        poly_actor_->GetProperty()->SetRepresentationToSurface();
        poly_actor_->GetProperty()->SetEdgeVisibility(false);
        poly_actor_->GetProperty()->SetOpacity(0.75);
        line_actor_->SetVisibility(true);
        break;
    case GeometryRenderStyle::Transparent50:
        poly_actor_->SetVisibility(true);
        poly_actor_->GetProperty()->SetRepresentationToSurface();
        poly_actor_->GetProperty()->SetEdgeVisibility(false);
        poly_actor_->GetProperty()->SetOpacity(0.50);
        line_actor_->SetVisibility(true);
        break;
    case GeometryRenderStyle::Transparent25:
        poly_actor_->SetVisibility(true);
        poly_actor_->GetProperty()->SetRepresentationToSurface();
        poly_actor_->GetProperty()->SetEdgeVisibility(false);
        poly_actor_->GetProperty()->SetOpacity(0.25);
        line_actor_->SetVisibility(true);
        break;
    case GeometryRenderStyle::WireframeWithLines:
        poly_actor_->SetVisibility(true);
        poly_actor_->GetProperty()->SetRepresentationToSurface();
        poly_actor_->GetProperty()->SetEdgeVisibility(true);
        poly_actor_->GetProperty()->SetLineWidth(1);
        poly_actor_->GetProperty()->SetOpacity(0.1);
        line_actor_->SetVisibility(true);
        break;
    case GeometryRenderStyle::Wireframe:
        poly_actor_->SetVisibility(true);
        poly_actor_->GetProperty()->SetRepresentationToSurface();
        poly_actor_->GetProperty()->SetEdgeVisibility(false);
        poly_actor_->GetProperty()->SetOpacity(0.001);
        line_actor_->SetVisibility(true);
        break;
    default:
        break;
    }
}
