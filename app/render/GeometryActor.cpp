#include "GeometryActor.h"
#include "Core.h"
#include "GeometryTopologyDiagnosticActor.h"
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepTools.hxx>
#include <IVtkTools_ShapeDataSource.hxx>
#include <IVtkVTK_ShapeData.hxx>
#include <IVtk_Types.hxx>
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

#include <chrono>
#include <cmath>
#include <spdlog/spdlog.h>
#include <stdexcept>
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

void GeometryActor::loadShape(const GeometryDataVtk& geometry_data, int precision)
{
    // 只复制一次拓扑供显示和拾取共用，隔离模型及撤销前像的三角缓存。
    BRepBuilderAPI_Copy copy(geometry_data.shape, false, false);
    if (!copy.IsDone())
        throw std::runtime_error("无法创建几何渲染副本");
    occ_shape_ = new IVtkOCC_Shape(copy.Shape());
    occ_shape_->SetId(static_cast<IVtk_IdType>(geometry_data.component_id));
    picking_geometry_index_.reset();
    if (geometry_data.geometry_index) {
        const auto& original = *geometry_data.geometry_index;
        picking_geometry_index_ = std::make_unique<GeometrySubshapeIndex>();
        auto& picking = *picking_geometry_index_;
        // 身份向量直接沿用；拓扑映射只构造一次，不先复制再清空。
        picking.vertex_local_to_global = original.vertex_local_to_global;
        picking.edge_local_to_global = original.edge_local_to_global;
        picking.face_local_to_global = original.face_local_to_global;
        picking.solid_local_to_global = original.solid_local_to_global;
        for (int type = 0; type < GeometrySubshapeIndex::kTypeCount; ++type) {
            const auto& original_map = original.type_maps[type];
            for (int local = 1; local <= original_map.Extent(); ++local)
                picking.type_maps[type].Add(copy.ModifiedShape(original_map.FindKey(local)));
        }
        picking.built = original.built;
    }
    rebuildMesh(precision);
    topology_diagnostics_->loadShape(occ_shape_->GetShape(), occ_shape_,
        line_only_, poly_only_);
    createActors();
}

void GeometryActor::setSurfacePrecision(int precision)
{
    rebuildMesh(precision);
    topology_diagnostics_->refreshDisplay(line_only_, poly_only_);
}

void GeometryActor::rebuildMesh(int precision)
{
    const auto started = std::chrono::steady_clock::now();
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

    auto* mesh_types = vtkIdTypeArray::SafeDownCast(
        src->GetCellData()->GetArray(IVtkVTK_ShapeData::ARRNAME_MESH_TYPES()));
    auto* subshape_ids = vtkIdTypeArray::SafeDownCast(
        src->GetCellData()->GetArray(IVtkVTK_ShapeData::ARRNAME_SUBSHAPE_IDS()));
    if (!mesh_types || !subshape_ids)
        throw std::runtime_error("OCCT 显示网格缺少类型或子形状身份数组");

    // OCCT 面边和拓扑边使用相同类型，结合归属身份单轮分类，避免两轮过滤。
    vtkNew<vtkIdList> line_cells;
    vtkNew<vtkIdList> face_cells;
    line_cells->Allocate(nV + nL);
    face_cells->Allocate(nP);
    for (vtkIdType cell_id = 0; cell_id < src->GetNumberOfCells(); ++cell_id) {
        const auto mesh_type = static_cast<IVtk_MeshType>(mesh_types->GetValue(cell_id));
        vtkIdList* output = nullptr;
        if (mesh_type == MT_ShadedFace) {
            output = face_cells;
        } else if (mesh_type == MT_FreeVertex || mesh_type == MT_SharedVertex
            || mesh_type == MT_FreeEdge || mesh_type == MT_BoundaryEdge
            || mesh_type == MT_SharedEdge || mesh_type == MT_SeamEdge) {
            // OCCT 还会用面 ID 重复输出同类型的边；只显示真正归属点、边的单元。
            const auto owner_type = occ_shape_->GetSubShape(subshape_ids->GetValue(cell_id)).ShapeType();
            if (owner_type == TopAbs_VERTEX || owner_type == TopAbs_EDGE)
                output = line_cells;
        }
        if (output)
            output->InsertNextId(cell_id);
    }
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

    const double elapsed_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started)
                                  .count();
    spdlog::debug("[SurfacePrecision] component={} level={} triangles={} rebuild_ms={:.1f}",
        occ_shape_->GetId(), precision, nP, elapsed_ms);
}

void GeometryActor::createActors()
{
    vtkNew<vtkPolyDataMapper> poly_mapper;
    poly_mapper->SetInputData(poly_only_);
    poly_mapper->SetRelativeCoincidentTopologyPolygonOffsetParameters(0.0, 1.0);

    poly_actor_->SetMapper(poly_mapper);
    poly_actor_->GetProperty()->SetColor(200.0 / 255.0, 200.0 / 255.0, 200.0 / 255.0);
    poly_actor_->GetProperty()->SetRepresentationToSurface();
    poly_actor_->GetProperty()->SetLineWidth(1);
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
    double opacity = 1.0;
    bool show_topology_edges = true;
    bool show_mesh_edges = false;
    switch (style_) {
    case GeometryRenderStyle::Surface:
        show_topology_edges = false;
        break;
    case GeometryRenderStyle::Transparent75:
        opacity = 0.75;
        break;
    case GeometryRenderStyle::Transparent50:
        opacity = 0.50;
        break;
    case GeometryRenderStyle::Transparent25:
        opacity = 0.25;
        break;
    case GeometryRenderStyle::WireframeWithLines:
        opacity = 0.1;
        show_mesh_edges = true;
        break;
    case GeometryRenderStyle::Wireframe:
        opacity = 0.001;
        break;
    default:
        break;
    }
    poly_actor_->SetVisibility(isVisible());
    line_actor_->SetVisibility(isVisible() && show_topology_edges);
    poly_actor_->GetProperty()->SetEdgeVisibility(show_mesh_edges);
    poly_actor_->GetProperty()->SetOpacity(opacity);
}
