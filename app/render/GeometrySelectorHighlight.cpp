#include "GeometrySelectorHighlight.h"
#include "CoincidentTopology.h"
#include "Core.h"
#include "GeometryActorSelectOp.h"

#include <IVtkTools_ShapePicker.hxx>
#include <IVtkTools_SubPolyDataFilter.hxx>
#include <IVtk_Types.hxx>
#include <NCollection_List.hxx>
#include <vtkActor.h>
#include <vtkMapper.h>
#include <vtkPartitionedDataSet.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

// 把共享高亮 actor 挂到当前 selector 的 mapper 上，并按模式套用完整样式（避免残留上一模式属性）
static void mountLineHighlight(vtkActor& actor, vtkMapper& mapper)
{
    actor.SetMapper(&mapper);

    // 相对显示 mapper 默认值
    mapper.SetRelativeCoincidentTopologyLineOffsetParameters(0, highlight::LINE_UNITS);
    mapper.SetRelativeCoincidentTopologyPointOffsetParameter(highlight::POINT_UNITS);

    vtkNew<vtkProperty> prop;
    prop->SetColor(1.0, 0.0, 0.0);
    prop->SetOpacity(0.5);
    prop->RenderLinesAsTubesOn();
    prop->SetLineWidth(3.0);
    prop->SetPointSize(8.0);
    prop->LightingOff();

    actor.SetProperty(prop);
}

static void mountFaceHighlight(vtkActor& actor, vtkMapper& mapper)
{
    actor.SetMapper(&mapper);

    // 相对显示 mapper 默认值
    mapper.SetRelativeCoincidentTopologyPolygonOffsetParameters(0, highlight::POLYGON_UNITS);

    vtkNew<vtkProperty> prop;
    prop->SetColor(1.0, 0.0, 0.0);
    prop->SetOpacity(1.0);
    actor.SetProperty(prop);
}

static void updateFilterAndNotify(IVtkTools_SubPolyDataFilter* filter, const NCollection_List<IVtk_IdType>& ids,
    vtkPartitionedDataSet* data)
{
    if (filter) {
        filter->SetData(ids);
        filter->Modified();
        filter->Update();
    }
    if (data)
        data->Modified();
}

// ─── Face ──────────────────────────────────────────────

GeometryFaceSelectorHighlight::GeometryFaceSelectorHighlight(vtkRenderer& renderer,
    vtkPartitionedDataSet& highlight_data, unsigned int partition_id,
    GeometryActorSelectOp select_op, vtkSmartPointer<IVtkTools_ShapePicker> picker)
    : renderer_(&renderer)
    , highlight_data_(&highlight_data)
    , partition_id_(partition_id)
    , select_op_(std::move(select_op))
    , picker_(std::move(picker))
{
    hl_filter_ = select_op_.buildHighlight(SelectMode::GeometryFace);
    if (hl_filter_) {
        hl_filter_->Update();
        highlight_data_->SetPartition(partition_id_, hl_filter_->GetOutput());
    }
    select_op_.configurePicker(picker_, SelectMode::GeometryFace);
}

GeometryFaceSelectorHighlight::~GeometryFaceSelectorHighlight()
{
    highlight_data_->SetPartition(partition_id_, nullptr);
}

void GeometryFaceSelectorHighlight::clear()
{
    selections_.clear();
    updateFilterAndNotify(hl_filter_, NCollection_List<IVtk_IdType>(), highlight_data_);
}

GeometrySelectionVtk GeometryFaceSelectorHighlight::get() const
{
    GeometrySelectionVtk s;
    s.type = ElementEnum::GeometryFace;
    s.component_id = -1;
    for (const auto& [subId, geomId] : selections_)
        s.ids.push_back(geomId);
    return s;
}

void GeometryFaceSelectorHighlight::select(double posx, double posy)
{
    IVtk_IdType subId = -1;
    auto geomId = select_op_.pickSubshape(picker_, renderer_, posx, posy, SelectMode::GeometryFace, subId);
    if (!geomId)
        return;

    auto [it, inserted] = selections_.insert_or_assign(subId, *geomId);
    if (!inserted)
        selections_.erase(it);

    NCollection_List<IVtk_IdType> ids;
    for (const auto& [sid, gid] : selections_)
        ids.Append(sid);
    updateFilterAndNotify(hl_filter_, ids, highlight_data_);
}

void GeometryFaceSelectorHighlight::setupHighlightStyle(vtkActor& actor, vtkMapper& mapper)
{
    mountFaceHighlight(actor, mapper);
}

// ─── Edge ──────────────────────────────────────────────

GeometryEdgeSelectorHighlight::GeometryEdgeSelectorHighlight(vtkRenderer& renderer,
    vtkPartitionedDataSet& highlight_data, unsigned int partition_id,
    GeometryActorSelectOp select_op, vtkSmartPointer<IVtkTools_ShapePicker> picker)
    : renderer_(&renderer)
    , highlight_data_(&highlight_data)
    , partition_id_(partition_id)
    , select_op_(std::move(select_op))
    , picker_(std::move(picker))
{
    hl_filter_ = select_op_.buildHighlight(SelectMode::GeometryEdge);
    if (hl_filter_) {
        hl_filter_->Update();
        highlight_data_->SetPartition(partition_id_, hl_filter_->GetOutput());
    }
    select_op_.configurePicker(picker_, SelectMode::GeometryEdge);
}

GeometryEdgeSelectorHighlight::~GeometryEdgeSelectorHighlight()
{
    highlight_data_->SetPartition(partition_id_, nullptr);
}

void GeometryEdgeSelectorHighlight::clear()
{
    selections_.clear();
    updateFilterAndNotify(hl_filter_, NCollection_List<IVtk_IdType>(), highlight_data_);
}

GeometrySelectionVtk GeometryEdgeSelectorHighlight::get() const
{
    GeometrySelectionVtk s;
    s.type = ElementEnum::GeometryEdge;
    s.component_id = -1;
    for (const auto& [subId, geomId] : selections_)
        s.ids.push_back(geomId);
    return s;
}

void GeometryEdgeSelectorHighlight::select(double posx, double posy)
{
    IVtk_IdType subId = -1;
    auto geomId = select_op_.pickSubshape(picker_, renderer_, posx, posy, SelectMode::GeometryEdge, subId);
    if (!geomId)
        return;

    auto [it, inserted] = selections_.insert_or_assign(subId, *geomId);
    if (!inserted)
        selections_.erase(it);

    NCollection_List<IVtk_IdType> ids;
    for (const auto& [sid, gid] : selections_)
        ids.Append(sid);
    updateFilterAndNotify(hl_filter_, ids, highlight_data_);
}

void GeometryEdgeSelectorHighlight::setupHighlightStyle(vtkActor& actor, vtkMapper& mapper)
{
    mountLineHighlight(actor, mapper);
}

// ─── Vertex ────────────────────────────────────────────

GeometryVertexSelectorHighlight::GeometryVertexSelectorHighlight(vtkRenderer& renderer,
    vtkPartitionedDataSet& highlight_data, unsigned int partition_id,
    GeometryActorSelectOp select_op, vtkSmartPointer<IVtkTools_ShapePicker> picker)
    : renderer_(&renderer)
    , highlight_data_(&highlight_data)
    , partition_id_(partition_id)
    , select_op_(std::move(select_op))
    , picker_(std::move(picker))
{
    hl_filter_ = select_op_.buildHighlight(SelectMode::GeometryVertex);
    if (hl_filter_) {
        hl_filter_->Update();
        highlight_data_->SetPartition(partition_id_, hl_filter_->GetOutput());
    }
    select_op_.configurePicker(picker_, SelectMode::GeometryVertex);
}

GeometryVertexSelectorHighlight::~GeometryVertexSelectorHighlight()
{
    highlight_data_->SetPartition(partition_id_, nullptr);
}

void GeometryVertexSelectorHighlight::clear()
{
    selections_.clear();
    updateFilterAndNotify(hl_filter_, NCollection_List<IVtk_IdType>(), highlight_data_);
}

GeometrySelectionVtk GeometryVertexSelectorHighlight::get() const
{
    GeometrySelectionVtk s;
    s.type = ElementEnum::GeometryVertex;
    s.component_id = -1;
    for (const auto& [subId, geomId] : selections_)
        s.ids.push_back(geomId);
    return s;
}

void GeometryVertexSelectorHighlight::select(double posx, double posy)
{
    IVtk_IdType subId = -1;
    auto geomId = select_op_.pickSubshape(picker_, renderer_, posx, posy, SelectMode::GeometryVertex, subId);
    if (!geomId)
        return;

    auto [it, inserted] = selections_.insert_or_assign(subId, *geomId);
    if (!inserted)
        selections_.erase(it);

    NCollection_List<IVtk_IdType> ids;
    for (const auto& [sid, gid] : selections_)
        ids.Append(sid);
    updateFilterAndNotify(hl_filter_, ids, highlight_data_);
}

void GeometryVertexSelectorHighlight::setupHighlightStyle(vtkActor& actor, vtkMapper& mapper)
{
    mountLineHighlight(actor, mapper);
}

// ─── Solid ─────────────────────────────────────────────

GeometrySolidSelectorHighlight::GeometrySolidSelectorHighlight(vtkRenderer& renderer,
    vtkPartitionedDataSet& highlight_data, unsigned int partition_id,
    GeometryActorSelectOp select_op, vtkSmartPointer<IVtkTools_ShapePicker> picker)
    : renderer_(&renderer)
    , highlight_data_(&highlight_data)
    , partition_id_(partition_id)
    , select_op_(std::move(select_op))
    , picker_(std::move(picker))
{
    hl_filter_ = select_op_.buildHighlight(SelectMode::GeometrySolid);
    if (hl_filter_) {
        hl_filter_->Update();
        highlight_data_->SetPartition(partition_id_, hl_filter_->GetOutput());
    }
    select_op_.configurePicker(picker_, SelectMode::GeometrySolid);
}

GeometrySolidSelectorHighlight::~GeometrySolidSelectorHighlight()
{
    highlight_data_->SetPartition(partition_id_, nullptr);
}

void GeometrySolidSelectorHighlight::clear()
{
    selections_.clear();
    highlighted_face_ids_.clear();
    updateFilterAndNotify(hl_filter_, NCollection_List<IVtk_IdType>(), highlight_data_);
}

GeometrySelectionVtk GeometrySolidSelectorHighlight::get() const
{
    GeometrySelectionVtk s;
    s.type = ElementEnum::GeometrySolid;
    s.component_id = -1;
    for (const auto& [subId, geomId] : selections_)
        s.ids.push_back(geomId);
    return s;
}

void GeometrySolidSelectorHighlight::select(double posx, double posy)
{
    GeomSolidId gid = kInvalidGeomSolidId;
    std::vector<IVtk_IdType> faceSubIds;
    if (!select_op_.pickSolid(picker_, renderer_, posx, posy, gid, faceSubIds))
        return;

    auto [it, inserted] = selections_.insert_or_assign(gid, gid);
    if (!inserted)
        selections_.erase(it);

    for (IVtk_IdType faceSubId : faceSubIds) {
        if (!inserted)
            highlighted_face_ids_.erase(faceSubId);
        else
            highlighted_face_ids_.insert(faceSubId);
    }

    NCollection_List<IVtk_IdType> ids;
    for (const auto& fid : highlighted_face_ids_)
        ids.Append(fid);
    updateFilterAndNotify(hl_filter_, ids, highlight_data_);
}

void GeometrySolidSelectorHighlight::setupHighlightStyle(vtkActor& actor, vtkMapper& mapper)
{
    mountLineHighlight(actor, mapper);
}
