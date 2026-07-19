#include "GeometryActor.h"
#include "Core.h"
#include <BRep_Builder.hxx>
#include <IVTKTools_ShapeDataSource.hxx>
#include <IVtkTools_SubPolyDataFilter.hxx>
#include <IVtkVTK_ShapeData.hxx>
#include <TopoDS_Compound.hxx>
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

#include <spdlog/spdlog.h>

// 将独立点、边、面包装成仅用于显示拾取的 Compound，使根形状进入 OCCT 的子形状选择流程。
static TopoDS_Shape makeSelectableShape(const TopoDS_Shape& shape)
{
    const TopAbs_ShapeEnum type = shape.ShapeType();
    if (type != TopAbs_VERTEX && type != TopAbs_EDGE && type != TopAbs_FACE)
        return shape;

    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, shape);
    return compound;
}

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
    // GeometryData 仍保留原始根形状；临时 Compound 只服务于 VTK 显示和拾取。
    const TopoDS_Shape selectable_shape = makeSelectableShape(geometry_data.shape);
    OccShapeHandle aShapeImpl = new IVtkOCC_Shape(selectable_shape);
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
    // 重载已有 Component 时复用 Actor，只更新 Mapper，避免 Renderer 重复登记同一对象。
    if (!renderer_->HasViewProp(poly_actor_))
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
    if (!renderer_->HasViewProp(line_actor_))
        renderer_->AddActor(line_actor_);

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

bool GeometryActor::isVisible() const
{
    return visibility_;
}


void GeometryActor::setRenderMode(GeometryRenderMode render_mode)
{
}

void GeometryActor::setRenderEdge(bool is_render)
{
    this->edge_render = is_render;
    this->line_actor_->SetVisibility(is_render && this->visibility_);
}
