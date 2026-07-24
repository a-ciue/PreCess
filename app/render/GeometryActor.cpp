#include "GeometryActor.h"
#include "Core.h"
#include <IVTKTools_ShapeDataSource.hxx>
#include <IVtkTools_SubPolyDataFilter.hxx>
#include <IVtkVTK_ShapeData.hxx>
#include <TopoDS_Shape.hxx>
#include <TopExp_Explorer.hxx>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkExtractSelection.h>
#include <vtkGeometryFilter.h>
#include <vtkIdTypeArray.h>
#include <vtkSelection.h>
#include <vtkSelectionNode.h>
#include <vtkCellData.h>

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
    this->edge_render = false;
    this->visibility_ = true;
}

GeometryActor::~GeometryActor()
{
    deleteGeometryActor();
}

bool GeometryActor::getIsEdgeRender()
{
    return this->edge_render;
}

void GeometryActor::loadShape(const GeometryDataVtk& geometry_data)
{
    // GeometryData 保证根形状已经是严格一层扁平的 Compound。
    OccShapeHandle aShapeImpl = new IVtkOCC_Shape(geometry_data.shape);
    aShapeImpl->SetId(static_cast<IVtk_IdType>(geometry_data.component_id));
    this->occ_shape_ = aShapeImpl;
    this->geometry_index_ = geometry_data.geometry_index;
    vtkSmartPointer<IVtkTools_ShapeDataSource> DS = vtkSmartPointer<IVtkTools_ShapeDataSource>::New();
    DS->SetShape(aShapeImpl);
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

    NCollection_Map<IVtk_IdType> edgeVertexIds;
    for (TopExp_Explorer exp(geometry_data.shape, TopAbs_EDGE); exp.More(); exp.Next()) {
        IVtk_IdType id = aShapeImpl->GetSubShapeId(exp.Current());
        if (id >= 0) edgeVertexIds.Add(id);
    }
    for (TopExp_Explorer exp(geometry_data.shape, TopAbs_VERTEX); exp.More(); exp.Next()) {
        IVtk_IdType id = aShapeImpl->GetSubShapeId(exp.Current());
        if (id >= 0) edgeVertexIds.Add(id);
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
}

bool GeometryActor::isVisible() const
{
    return visibility_;
}

void GeometryActor::setRenderEdge(bool is_render)
{
    this->edge_render = is_render;
    applyStyle();
}

void GeometryActor::setRenderStyle(GeometryRenderStyle style)
{
    this->style_ = style;
    applyStyle();
}

GeometryRenderStyle GeometryActor::getRenderStyle() const
{
    return style_;
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