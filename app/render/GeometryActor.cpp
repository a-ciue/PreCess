#include "GeometryActor.h"
#include "Core.h"
#include <IVTKTools_ShapeDataSource.hxx>
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

#include <cstring>
#include <spdlog/spdlog.h>

static vtkSmartPointer<vtkDataArray> findSubIdArray(vtkPolyData* pd, const OccShapeHandle& occShape)
{
    if (!pd || occShape.IsNull())
        return nullptr;

    vtkCellData* cd = pd->GetCellData();
    if (!cd)
        return nullptr;

    const vtkIdType nCells = pd->GetNumberOfCells();
    if (nCells <= 0)
        return nullptr;

    vtkDataArray* best = nullptr;
    vtkIdType bestValidCount = 0;
    bool bestHasLineValid = false;

    for (int i = 0; i < cd->GetNumberOfArrays(); ++i) {
        vtkDataArray* a = cd->GetArray(i);
        if (!a)
            continue;
        if (a->GetNumberOfComponents() != 1)
            continue;
        if (a->GetNumberOfTuples() != nCells)
            continue;

        const char* name = a->GetName();
        if (name) {
            if (std::strcmp(name, "vtkOriginalCellIds") == 0)
                continue;
            if (std::strcmp(name, "vtkOriginalPointIds") == 0)
                continue;
        }

        vtkIdType validCount = 0;
        bool hasLineValid = false;
        for (vtkIdType c = 0; c < nCells; ++c) {
            IVtk_IdType sid = static_cast<IVtk_IdType>(a->GetTuple1(c));
            if (sid > 0) {
                try {
                    if (!occShape->GetSubShape(sid).IsNull()) {
                        ++validCount;
                        if (!hasLineValid && pd->GetCellType(c) == VTK_LINE)
                            hasLineValid = true;
                    }
                } catch (...) {
                }
            }
        }

        bool better = false;
        if (!best) {
            better = true;
        } else if (hasLineValid && !bestHasLineValid) {
            better = true;
        } else if (!hasLineValid && bestHasLineValid) {
        } else if (validCount > bestValidCount) {
            better = true;
        }

        if (better) {
            bestValidCount = validCount;
            bestHasLineValid = hasLineValid;
            best = a;
        }
    }

    if (best && bestValidCount > 0) {
        spdlog::info("[GeometryActor] found subId array '{}' valid={}/{} hasLine={}",
            (best->GetName() ? best->GetName() : "(null)"),
            static_cast<int>(bestValidCount), static_cast<int>(nCells),
            bestHasLineValid ? "yes" : "no");
        return best;
    }

    spdlog::warn("[GeometryActor] no valid subId array found in polydata with {} cells", static_cast<int>(nCells));
    return nullptr;
}

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
    // 全局：开一次
    vtkMapper::SetResolveCoincidentTopologyToPolygonOffset();

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

    line_sub_id_array_ = findSubIdArray(line_only, occ_shape_);
    poly_sub_id_array_ = findSubIdArray(poly_only, occ_shape_);

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
    poly_mapper->SetRelativeCoincidentTopologyPolygonOffsetParameters(0, 0);

    poly_actor_->SetMapper(poly_mapper);
    poly_actor_->GetProperty()->SetColor(200.0 / 255.0, 200.0 / 255.0, 200.0 / 255.0);
    renderer_->AddActor(poly_actor_);

    poly_hl_filter_ = vtkSmartPointer<IVtkTools_SubPolyDataFilter>::New();
    poly_hl_filter_->SetInputData(poly_only);
    poly_hl_filter_->SetDoFiltering(true);
    if (poly_sub_id_array_ && poly_sub_id_array_->GetName())
        poly_hl_filter_->SetIdsArrayName(poly_sub_id_array_->GetName());

    poly_hl_mapper_ = vtkSmartPointer<vtkPolyDataMapper>::New();
    poly_hl_mapper_->SetInputConnection(poly_hl_filter_->GetOutputPort());
    // 相对显示 mapper 默认值：多边形 (0,-1)
    poly_hl_mapper_->SetRelativeCoincidentTopologyPolygonOffsetParameters(0, -0.5);


    poly_hl_actor_ = vtkSmartPointer<vtkActor>::New();
    poly_hl_actor_->SetMapper(poly_hl_mapper_);
    poly_hl_actor_->GetProperty()->SetColor(1.0, 0.0, 0.0);
    poly_hl_actor_->GetProperty()->SetOpacity(1);
    poly_hl_actor_->SetVisibility(false);
    poly_hl_actor_->PickableOff();
    renderer_->AddActor(poly_hl_actor_);

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

    line_hl_filter_ = vtkSmartPointer<IVtkTools_SubPolyDataFilter>::New();
    line_hl_filter_->SetInputData(line_only);
    line_hl_filter_->SetDoFiltering(true);
    if (line_sub_id_array_ && line_sub_id_array_->GetName())
        line_hl_filter_->SetIdsArrayName(line_sub_id_array_->GetName());

    line_hl_mapper_ = vtkSmartPointer<vtkPolyDataMapper>::New();
    line_hl_mapper_->SetInputConnection(line_hl_filter_->GetOutputPort());
    // 相对显示 mapper 默认值：线 (0,-5)，点 (-10)
    line_hl_mapper_->SetRelativeCoincidentTopologyLineOffsetParameters(0, -1);
    line_hl_mapper_->SetRelativeCoincidentTopologyPointOffsetParameter(-2);

    line_hl_actor_ = vtkSmartPointer<vtkActor>::New();
    line_hl_actor_->SetMapper(line_hl_mapper_);
    line_hl_actor_->GetProperty()->SetColor(1.0, 0.0, 0.0);
    line_hl_actor_->GetProperty()->SetOpacity(0.5);
    line_hl_actor_->GetProperty()->RenderLinesAsTubesOn();
    line_hl_actor_->GetProperty()->SetLineWidth(3.0);
    line_hl_actor_->GetProperty()->SetPointSize(8.0);
    line_hl_actor_->GetProperty()->LightingOff();
    line_hl_actor_->SetVisibility(false);
    line_hl_actor_->PickableOff();
    renderer_->AddActor(line_hl_actor_);

    spdlog::info("[GeometryActor] component={} actors added, face_cells={} line_cells={}",
        geometry_data.component_id, static_cast<int>(poly_only->GetNumberOfCells()), static_cast<int>(line_only->GetNumberOfCells()));
}

void GeometryActor::deleteGeometryActor()
{
    if (this->renderer_) {
        renderer_->RemoveActor(this->poly_actor_);
        renderer_->RemoveActor(this->line_actor_);
        renderer_->RemoveActor(this->poly_hl_actor_);
        renderer_->RemoveActor(this->line_hl_actor_);
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

const GeometrySubshapeIndex* GeometryActor::geometryIndex() const noexcept
{
    return geometry_index_; 
}

const vtkDataArray* GeometryActor::lineSubIdArray() const noexcept
{
    return line_sub_id_array_.GetPointer();
}

const vtkDataArray* GeometryActor::polySubIdArray() const noexcept
{
    return poly_sub_id_array_.GetPointer();
}

vtkActor* GeometryActor::polyHLActor() noexcept
{
    return poly_hl_actor_.GetPointer();
}

const vtkActor* GeometryActor::polyHLActor() const noexcept
{
    return poly_hl_actor_.GetPointer();
}

vtkActor* GeometryActor::lineHLActor() noexcept
{
    return line_hl_actor_.GetPointer();
}

const vtkActor* GeometryActor::lineHLActor() const noexcept
{
    return line_hl_actor_.GetPointer();
}

IVtkTools_SubPolyDataFilter* GeometryActor::polyHLFilter() noexcept
{
    return poly_hl_filter_.GetPointer();
}

const IVtkTools_SubPolyDataFilter* GeometryActor::polyHLFilter() const noexcept
{
    return poly_hl_filter_.GetPointer();
}

IVtkTools_SubPolyDataFilter* GeometryActor::lineHLFilter() noexcept
{
    return line_hl_filter_.GetPointer();
}

const IVtkTools_SubPolyDataFilter* GeometryActor::lineHLFilter() const noexcept
{
    return line_hl_filter_.GetPointer();
}