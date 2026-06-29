#include "GeometryActor.h"
#include "Core.h"
#include <IVTKTools_ShapeDataSource.hxx>
#include <TopoDS_Shape.hxx>
#include <vtkPointData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkExtractSelection.h>
#include <vtkGeometryFilter.h>
#include <vtkIdTypeArray.h>
#include <vtkSelection.h>
#include <vtkSelectionNode.h>
#include <vtkUnsignedCharArray.h>
#include <vtkCellData.h>

#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>
GeometryActor::GeometryActor(vtkRenderer* renderer, GeometryRenderMode render_mode)
{
    this->renderer_ = renderer;
    this->render_mode_ = render_mode;
    this->edge_render = false;
    this->visibility_ = true;
}

GeometryActor::~GeometryActor()
{
    deleteGeometryActor();
}

GeometryRenderMode GeometryActor::getGeometryRenderMode()
{
    return this->render_mode_;
}

bool GeometryActor::getIsEdgeRender()
{
    return this->edge_render;
}

void GeometryActor::loadShape(const GeometryDataVtk& geometry_data)
{
    OccShapeHandle aShapeImpl = new IVtkOCC_Shape(geometry_data.shape);
    aShapeImpl->SetId(static_cast<IVtk_IdType>(geometry_data.component_id));
    this->occ_shape_ = aShapeImpl;
    vtkSmartPointer<IVtkTools_ShapeDataSource> DS = vtkSmartPointer<IVtkTools_ShapeDataSource>::New();
    DS->SetShape(aShapeImpl);
    DS->Update();
    vtkPolyData* src = DS->GetOutput();

    const vtkIdType nV = src->GetNumberOfVerts();
    const vtkIdType nL = src->GetNumberOfLines();
    const vtkIdType nP = src->GetNumberOfPolys();

    spdlog::info("[GeometryActor] loadShape component={} verts={} lines={} polys={}",
        geometry_data.component_id, static_cast<int>(nV), static_cast<int>(nL), static_cast<int>(nP));

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

    auto EnsureCellRgbScalars = [](vtkPolyData* pd, const unsigned char rgb[3]) -> vtkUnsignedCharArray* {
        if (!pd)
            return nullptr;
        vtkCellData* cd = pd->GetCellData();
        if (!cd)
            return nullptr;

        vtkUnsignedCharArray* colors = vtkUnsignedCharArray::SafeDownCast(cd->GetScalars());
        if (!colors || colors->GetNumberOfComponents() != 3 || colors->GetNumberOfTuples() != pd->GetNumberOfCells()) {
            vtkNew<vtkUnsignedCharArray> c;
            c->SetName("CellColors");
            c->SetNumberOfComponents(3);
            c->SetNumberOfTuples(pd->GetNumberOfCells());
            colors = c.GetPointer();
            cd->SetScalars(c);
        }

        for (vtkIdType i = 0; i < pd->GetNumberOfCells(); ++i)
            colors->SetTypedTuple(i, rgb);

        colors->Modified();
        pd->GetCellData()->Modified();
        pd->Modified();
        return colors;
    };

    unsigned char faceBase[3] = { 200, 200, 200 };
    if (this->poly_actor_ && this->poly_actor_->GetProperty()) {
        double c[3];
        this->poly_actor_->GetProperty()->GetColor(c);
        faceBase[0] = static_cast<unsigned char>(std::clamp(static_cast<int>(std::lround(c[0] * 255.0)), 0, 255));
        faceBase[1] = static_cast<unsigned char>(std::clamp(static_cast<int>(std::lround(c[1] * 255.0)), 0, 255));
        faceBase[2] = static_cast<unsigned char>(std::clamp(static_cast<int>(std::lround(c[2] * 255.0)), 0, 255));
    }

    const unsigned char lineBase[3] = { 0, 0, 0 };

    EnsureCellRgbScalars(poly_only, faceBase);
    EnsureCellRgbScalars(line_only, lineBase);

    vtkNew<vtkPolyDataMapper> poly_mapper;
    poly_mapper->SetInputData(poly_only);
    poly_mapper->ScalarVisibilityOn();
    poly_mapper->SetScalarModeToUseCellData();
    poly_mapper->SetColorModeToDirectScalars();

    this->poly_actor_->SetMapper(poly_mapper);
    this->renderer_->AddActor(this->poly_actor_);

    vtkNew<vtkPolyDataMapper> line_mapper;
    line_mapper->SetInputData(line_only);
    line_mapper->ScalarVisibilityOn();
    line_mapper->SetScalarModeToUseCellData();
    line_mapper->SetColorModeToDirectScalars();
    line_mapper->SetRelativeCoincidentTopologyLineOffsetParameters(0, -0.1);

    this->line_actor_->SetMapper(line_mapper);
    this->line_actor_->GetProperty()->LightingOff();
    this->line_actor_->GetProperty()->SetLineWidth(2.0);
    this->line_actor_->GetProperty()->RenderLinesAsTubesOn();
    this->line_actor_->GetProperty()->SetPointSize(6.0);

    this->renderer_->AddActor(this->line_actor_);

    spdlog::info("[GeometryActor] component={} actors added, face_cells={} line_cells={}",
        geometry_data.component_id, static_cast<int>(poly_only->GetNumberOfCells()), static_cast<int>(line_only->GetNumberOfCells()));
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
    this->poly_actor_->SetVisibility(visibility);
    this->line_actor_->SetVisibility(visibility);
    this->visibility_ = visibility;
}

void GeometryActor::setRenderMode(GeometryRenderMode render_mode)
{
}

void GeometryActor::setRenderEdge(bool is_render)
{
    this->edge_render = is_render;
    this->line_actor_->SetVisibility(is_render && this->visibility_);
}

vtkActor* GeometryActor::polyActor() noexcept 
{ 
    return poly_actor_.GetPointer();
}

const vtkActor* GeometryActor::polyActor() const noexcept 
{ 
    return poly_actor_.GetPointer();
}

vtkActor* GeometryActor::lineActor() noexcept 
{ 
    return line_actor_.GetPointer();
}

const vtkActor* GeometryActor::lineActor() const noexcept 
{ 
    return line_actor_.GetPointer();
}

const OccShapeHandle& GeometryActor::getOccShape() const 
{ 
    return occ_shape_;
}