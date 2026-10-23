#include "GeometryActor.h"
#include "Core.h"
#include "GeometryTopologyDiagnosticActor.h"
#include <BRepBuilderAPI_Copy.hxx>
#include <IVtkTools_ShapeDataSource.hxx>
#include <IVtkTools_SubPolyDataFilter.hxx>
#include <IVtkVTK_ShapeData.hxx>
#include <Prs3d_Drawer.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS_Shape.hxx>
#include <unordered_map>
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

void GeometryActor::loadShape(const GeometryDataVtk& geometry_data, int precision)
{
    if (!surface_precision::isValid(precision))
        throw std::invalid_argument("无效的曲面显示精细度");
    // 拾取使用独立且固定的标准精度副本，模型与撤销前像的三角缓存不可写。
    BRepBuilderAPI_Copy copy(geometry_data.shape, false, false);
    if (!copy.IsDone())
        throw std::runtime_error("无法创建几何拾取副本");
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
    vtkNew<IVtkTools_ShapeDataSource> picking_source;
    picking_source->SetShape(occ_shape_);
    picking_source->Update();
    auto display = prepareDisplay(precision, false);
    if (!line_only_)
        line_only_ = vtkSmartPointer<vtkPolyData>::New();
    if (!poly_only_)
        poly_only_ = vtkSmartPointer<vtkPolyData>::New();
    line_only_->ShallowCopy(display.lines);
    poly_only_->ShallowCopy(display.polygons);
    topology_diagnostics_->loadShape(occ_shape_->GetShape(), occ_shape_,
        line_only_, poly_only_);
    createActors();
}

GeometryActor::DisplayData GeometryActor::prepareDisplay(int precision, bool include_diagnostics) const
{
    const auto started = std::chrono::steady_clock::now();
    BRepBuilderAPI_Copy copy(occ_shape_->GetShape(), false, false);
    if (!copy.IsDone())
        throw std::runtime_error("无法创建曲面显示副本");
    const OccShapeHandle display_shape = new IVtkOCC_Shape(copy.Shape());
    const auto& settings = surface_precision::kLevels[static_cast<size_t>(precision)];
    display_shape->Attributes()->SetDeviationCoefficient(settings.deviation_coefficient);
    display_shape->Attributes()->SetDeviationAngle(settings.angle_degrees * std::acos(-1.0) / 180.0);
    vtkNew<IVtkTools_ShapeDataSource> source;
    source->SetShape(display_shape);
    source->Update();
    vtkPolyData* src = source->GetOutput();

    // 显示副本的遍历顺序不是身份契约：通过 Copy 历史显式映射到固定拾取 ID。
    TopTools_IndexedMapOfShape subshapes;
    TopExp::MapShapes(occ_shape_->GetShape(), subshapes);
    std::unordered_map<IVtk_IdType, IVtk_IdType> display_to_picking;
    for (int i = 1; i <= subshapes.Extent(); ++i) {
        const auto& original = subshapes.FindKey(i);
        const auto copied = copy.ModifiedShape(original);
        if (!copied.IsNull())
            display_to_picking.emplace(display_shape->GetSubShapeId(copied), occ_shape_->GetSubShapeId(original));
    }
    if (auto ids = findSubIdArray(src)) {
        for (vtkIdType i = 0; i < ids->GetNumberOfTuples(); ++i) {
            const auto found = display_to_picking.find(static_cast<IVtk_IdType>(ids->GetTuple1(i)));
            if (found == display_to_picking.end() || found->second < 0)
                throw std::runtime_error("曲面显示子形状无法映射到拾取身份");
            ids->SetTuple1(i, found->second);
        }
    } else if (src->GetNumberOfCells() != 0) {
        throw std::runtime_error("曲面显示缺少子形状身份数组");
    }
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

    DisplayData display { line_only, poly_only, {} };
    // 新几何（含替换加载）的诊断在 loadShape 计算；切档仅重新提取已有叠加。
    if (include_diagnostics)
        display.diagnostics = topology_diagnostics_->prepareDisplay(line_only, poly_only);
    const double elapsed_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - started)
                                  .count();
    spdlog::info("[SurfacePrecision] component={} level={} triangles={} rebuild_ms={:.1f}",
        occ_shape_->GetId(), precision, nP, elapsed_ms);
    return display;
}

GeometryActor::DisplayData GeometryActor::captureDisplay() const
{
    DisplayData display { vtkSmartPointer<vtkPolyData>::New(), vtkSmartPointer<vtkPolyData>::New(),
        topology_diagnostics_->captureDisplay() };
    display.lines->ShallowCopy(line_only_);
    display.polygons->ShallowCopy(poly_only_);
    return display;
}

void GeometryActor::commitDisplay(const DisplayData& display)
{
    line_only_->ShallowCopy(display.lines);
    poly_only_->ShallowCopy(display.polygons);
    topology_diagnostics_->commitDisplay(display.diagnostics, line_only_, poly_only_);
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
