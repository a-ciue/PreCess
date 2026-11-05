#include "GeometryActor.h"
#include "Core.h"
#include "GeometryTopologyDiagnosticActor.h"
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepTools.hxx>
#include <IVtkTools_ShapeDataSource.hxx>
#include <IVtkTools_SubPolyDataFilter.hxx>
#include <IVtkVTK_ShapeData.hxx>
#include <Prs3d_Drawer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>
#include <vtkExtractSelection.h>
#include <vtkGeometryFilter.h>
#include <vtkIdTypeArray.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSelection.h>
#include <vtkSelectionNode.h>

#include <chrono>
#include <cmath>
#include <spdlog/spdlog.h>
#include <stdexcept>

GeometryActor::GeometryActor(vtkRenderer* renderer)
{
    this->renderer_ = renderer;
    this->visibility_ = true;
    topology_diagnostics_ = std::make_unique<GeometryTopologyDiagnosticActor>(renderer_);
}

GeometryActor::~GeometryActor()
{
    deleteGeometryActor();
}

void GeometryActor::loadShape(const GeometryDataVtk& geometry_data, int precision)
{
    if (!surface_precision::isValid(precision))
        throw std::invalid_argument("无效的曲面显示精细度");
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
    surface_precision_ = -1;
    rebuildMesh(precision);
    topology_diagnostics_->loadShape(occ_shape_->GetShape(), occ_shape_,
        line_only_, poly_only_);
    createActors();
    surface_precision_ = precision;
}

void GeometryActor::setSurfacePrecision(int precision)
{
    if (!surface_precision::isValid(precision))
        throw std::invalid_argument("无效的曲面显示精细度");
    if (precision == surface_precision_)
        return;
    // 更新中可能部分写入，失效档位使再次选择旧档位也能重新构建。
    surface_precision_ = -1;
    rebuildMesh(precision);
    topology_diagnostics_->refreshDisplay(line_only_, poly_only_);
    surface_precision_ = precision;
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

    auto ExtractCellRangeToPolyData = [](vtkPolyData* in, vtkIdType start, vtkIdType count) -> vtkSmartPointer<vtkPolyData> {
        vtkNew<vtkIdTypeArray> ids;
        ids->SetNumberOfComponents(1);
        ids->SetNumberOfValues(count);
        for (vtkIdType i = 0; i < count; ++i)
            ids->SetValue(i, start + i);

        vtkNew<vtkSelectionNode> node;
        node->SetFieldType(vtkSelectionNode::CELL);
        node->SetContentType(vtkSelectionNode::INDICES);
        node->SetSelectionList(ids);

        vtkNew<vtkSelection> sel;
        sel->AddNode(node);

        vtkNew<vtkExtractSelection> extract;
        extract->SetInputData(0, in);
        extract->SetInputData(1, sel);
        extract->Update();

        vtkNew<vtkGeometryFilter> geom;
        geom->SetInputConnection(extract->GetOutputPort());
        geom->Update();

        return vtkPolyData::SafeDownCast(geom->GetOutput());
    };

    vtkSmartPointer<vtkPolyData> line_only = ExtractCellRangeToPolyData(src, 0, nV + nL);
    vtkSmartPointer<vtkPolyData> poly_only = ExtractCellRangeToPolyData(src, nV + nL, nP);

    if (!line_only)
        line_only = vtkSmartPointer<vtkPolyData>::New();
    if (!poly_only)
        poly_only = vtkSmartPointer<vtkPolyData>::New();

    const double elapsed_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started)
                                  .count();
    spdlog::debug("[SurfacePrecision] component={} level={} triangles={} rebuild_ms={:.1f}",
        occ_shape_->GetId(), precision, nP, elapsed_ms);
    if (!line_only_)
        line_only_ = vtkSmartPointer<vtkPolyData>::New();
    if (!poly_only_)
        poly_only_ = vtkSmartPointer<vtkPolyData>::New();
    line_only_->ShallowCopy(line_only);
    poly_only_->ShallowCopy(poly_only);
}

void GeometryActor::createActors()
{
    const auto& shape = occ_shape_->GetShape();
    NCollection_Map<IVtk_IdType> edgeVertexIds;
    for (TopExp_Explorer exp(shape, TopAbs_EDGE); exp.More(); exp.Next()) {
        IVtk_IdType id = occ_shape_->GetSubShapeId(exp.Current());
        if (id >= 0)
            edgeVertexIds.Add(id);
    }
    for (TopExp_Explorer exp(shape, TopAbs_VERTEX); exp.More(); exp.Next()) {
        IVtk_IdType id = occ_shape_->GetSubShapeId(exp.Current());
        if (id >= 0)
            edgeVertexIds.Add(id);
    }

    auto lineEdgeFilter = vtkSmartPointer<IVtkTools_SubPolyDataFilter>::New();
    lineEdgeFilter->SetInputData(line_only_);
    lineEdgeFilter->SetDoFiltering(true);
    // OCCT 只保存数组名裸指针，不能引用切档后会释放的旧 VTK 数组字符串。
    lineEdgeFilter->SetIdsArrayName(IVtkVTK_ShapeData::ARRNAME_SUBSHAPE_IDS());
    lineEdgeFilter->SetData(edgeVertexIds);

    vtkNew<vtkPolyDataMapper> poly_mapper;
    poly_mapper->SetInputData(poly_only_);
    poly_mapper->SetRelativeCoincidentTopologyPolygonOffsetParameters(0.0, 1.0);

    poly_actor_->SetMapper(poly_mapper);
    poly_actor_->GetProperty()->SetColor(200.0 / 255.0, 200.0 / 255.0, 200.0 / 255.0);
    renderer_->AddActor(poly_actor_);

    vtkNew<vtkPolyDataMapper> line_mapper;
    line_mapper->SetInputConnection(lineEdgeFilter->GetOutputPort());
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
