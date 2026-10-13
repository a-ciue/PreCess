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
#include <vtkCellData.h>
#include <vtkExtractSelection.h>
#include <vtkGeometryFilter.h>
#include <vtkIdTypeArray.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkSelection.h>
#include <vtkSelectionNode.h>

#include <array>
#include <chrono>
#include <cmath>
#include <spdlog/spdlog.h>
#include <stdexcept>

// 按 OCCT 约定的正式名称获取单元对应的子形状 ID 数组。
static vtkSmartPointer<vtkDataArray> findSubIdArray(vtkPolyData* poly_data)
{
    if (!poly_data)
        return nullptr;

    vtkCellData* cell_data = poly_data->GetCellData();
    if (!cell_data)
        return nullptr;

    return cell_data->GetArray(IVtkVTK_ShapeData::ARRNAME_SUBSHAPE_IDS());
}

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

void GeometryActor::loadShape(const GeometryDataVtk& geometry_data)
{
    // 只复制拓扑、共享不可变曲面；三角化及法向缓存不能写入模型或撤销快照的 TShape。
    BRepBuilderAPI_Copy copy(geometry_data.shape, false, false);
    if (!copy.IsDone())
        throw std::runtime_error("无法创建曲面显示副本");
    OccShapeHandle aShapeImpl = new IVtkOCC_Shape(copy.Shape());
    aShapeImpl->SetId(static_cast<IVtk_IdType>(geometry_data.component_id));
    this->occ_shape_ = aShapeImpl;
    geometry_index_ = nullptr;
    display_geometry_index_.reset();
    if (geometry_data.geometry_index) {
        display_geometry_index_ = std::make_unique<GeometrySubshapeIndex>(*geometry_data.geometry_index);
        // 按原 local id 显式映射，不能依赖复制后重新遍历的子形状顺序。
        for (int type = 0; type < GeometrySubshapeIndex::kTypeCount; ++type) {
            auto& display_map = display_geometry_index_->type_maps[type];
            display_map.Clear();
            const auto& original_map = geometry_data.geometry_index->type_maps[type];
            for (int local = 1; local <= original_map.Extent(); ++local)
                display_map.Add(copy.ModifiedShape(original_map.FindKey(local)));
        }
        geometry_index_ = display_geometry_index_.get();
    }
    rebuildShape(surface_precision_);
}

void GeometryActor::setSurfacePrecision(int precision)
{
    if (!surface_precision::isValid(precision)
        || precision == surface_precision_)
        return;
    if (!occ_shape_.IsNull()) {
        auto prepared = prepareSurfacePrecision(precision);
        swapDisplay(*prepared);
        return;
    }
    surface_precision_ = precision;
}

std::unique_ptr<GeometryActor> GeometryActor::prepareSurfacePrecision(int precision) const
{
    vtkNew<vtkRenderer> staging_renderer;
    auto prepared = std::make_unique<GeometryActor>(staging_renderer);
    prepared->surface_precision_ = precision;
    prepared->style_ = style_;
    prepared->visibility_ = visibility_;
    prepared->topology_diagnostics_->copySettingsFrom(*topology_diagnostics_);
    prepared->loadShape({ occ_shape_->GetShape(), static_cast<Index>(occ_shape_->GetId()), geometry_index_ });
    return prepared;
}

void GeometryActor::swapDisplay(GeometryActor& other)
{
    // 旧显示保留在候选对象中，提交失败时可直接交换回来，无需再次三角化。
    deleteGeometryActor();
    other.deleteGeometryActor();
    using std::swap;
    swap(occ_shape_, other.occ_shape_);
    swap(display_geometry_index_, other.display_geometry_index_);
    geometry_index_ = display_geometry_index_.get();
    other.geometry_index_ = other.display_geometry_index_.get();
    swap(poly_actor_, other.poly_actor_);
    swap(line_actor_, other.line_actor_);
    swap(line_sub_id_array_, other.line_sub_id_array_);
    swap(poly_sub_id_array_, other.poly_sub_id_array_);
    swap(line_only_, other.line_only_);
    swap(poly_only_, other.poly_only_);
    swap(topology_diagnostics_, other.topology_diagnostics_);
    swap(surface_precision_, other.surface_precision_);
    topology_diagnostics_->bindRender(renderer_);
    other.topology_diagnostics_->bindRender(other.renderer_);
    renderer_->AddActor(poly_actor_);
    renderer_->AddActor(line_actor_);
    other.renderer_->AddActor(other.poly_actor_);
    other.renderer_->AddActor(other.line_actor_);
}

void GeometryActor::rebuildShape(int precision)
{
    const auto started = std::chrono::steady_clock::now();
    const TopoDS_Shape& shape = occ_shape_->GetShape();
    // 精细网格也满足粗档误差，必须失效旧缓存，保证精细→粗糙确实减少面数。
    BRepTools::Clean(shape);
    const auto& settings = surface_precision::kLevels[static_cast<size_t>(precision)];
    occ_shape_->Attributes()->SetDeviationCoefficient(settings.deviation_coefficient);
    occ_shape_->Attributes()->SetDeviationAngle(settings.angle_degrees * std::acos(-1.0) / 180.0);
    vtkSmartPointer<IVtkTools_ShapeDataSource> DS = vtkSmartPointer<IVtkTools_ShapeDataSource>::New();
    DS->SetShape(occ_shape_);
    DS->Update();
    vtkPolyData* src = DS->GetOutput();

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

    this->line_only_ = line_only;
    this->poly_only_ = poly_only;

    line_sub_id_array_ = findSubIdArray(line_only);
    poly_sub_id_array_ = findSubIdArray(poly_only);

    topology_diagnostics_->loadShape(shape, occ_shape_,
        line_only_, line_sub_id_array_, poly_only_, poly_sub_id_array_);

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
    lineEdgeFilter->SetInputData(line_only);
    lineEdgeFilter->SetDoFiltering(true);
    if (line_sub_id_array_ && line_sub_id_array_->GetName())
        lineEdgeFilter->SetIdsArrayName(line_sub_id_array_->GetName());
    lineEdgeFilter->SetData(edgeVertexIds);

    vtkNew<vtkPolyDataMapper> poly_mapper;
    poly_mapper->SetInputData(poly_only);
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
    const double elapsed_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started)
                                  .count();
    spdlog::info("[SurfacePrecision] component={} level={} triangles={} rebuild_ms={:.1f}",
        occ_shape_->GetId(), precision, nP, elapsed_ms);
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
