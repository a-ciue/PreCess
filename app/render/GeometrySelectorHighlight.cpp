#include "GeometrySelectorHighlight.h"
#include "GeometryActorSelectOp.h"
#include "Core.h"

#include <IVtkTools_ShapePicker.hxx>
#include <IVtkTools_SubPolyDataFilter.hxx>
#include <IVtk_Types.hxx>
#include <NCollection_List.hxx>
#include <vtkActor.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>

static void flushHighlight(IVtkTools_SubPolyDataFilter* filter, vtkRenderer* renderer)
{
    if (!filter || !renderer)
        return;
    filter->Modified();
    if (renderer->GetRenderWindow())
        renderer->GetRenderWindow()->Render();
}

// 把共享高亮 actor 挂到当前 selector 的 mapper 上，并按模式套用完整样式（避免残留上一模式属性）
static void mountHighlight(vtkActor* actor, const GeometryHighlightPipeline& hl, SelectMode mode)
{
    if (!actor || !hl.mapper)
        return;

    actor->SetMapper(hl.mapper);

    vtkNew<vtkProperty> prop;
    prop->SetColor(1.0, 0.0, 0.0);
    if (mode == SelectMode::GeometryEdge || mode == SelectMode::GeometryVertex) {
        prop->SetOpacity(0.5);
        prop->RenderLinesAsTubesOn();
        prop->SetLineWidth(3.0);
        prop->SetPointSize(8.0);
        prop->LightingOff();
    } else {
        prop->SetOpacity(1.0);
    }
    actor->SetProperty(prop);
}

// ─── Face ──────────────────────────────────────────────

GeometryFaceSelectorHighlight::GeometryFaceSelectorHighlight(vtkRenderer& renderer, vtkActor& highlight_actor,
    GeometryActorSelectOp select_op, vtkSmartPointer<IVtkTools_ShapePicker> picker)
    : renderer_(&renderer)
    , highlight_actor_(&highlight_actor)
    , select_op_(std::move(select_op))
    , picker_(std::move(picker))
{
    hl_ = select_op_.buildHighlight(SelectMode::GeometryFace);
    mountHighlight(highlight_actor_, hl_, SelectMode::GeometryFace);
    select_op_.configurePicker(picker_, SelectMode::GeometryFace);
}

GeometryFaceSelectorHighlight::~GeometryFaceSelectorHighlight()
{
    clear();
}

void GeometryFaceSelectorHighlight::clear()
{
    selections_.clear();
    if (hl_.filter) {
        hl_.filter->Clear();
        hl_.filter->Modified();
    }
    if (renderer_ && renderer_->GetRenderWindow())
        renderer_->GetRenderWindow()->Render();
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

    if (hl_.filter)
        hl_.filter->SetData(ids);
    flushHighlight(hl_.filter, renderer_);
}

// ─── Edge ──────────────────────────────────────────────

GeometryEdgeSelectorHighlight::GeometryEdgeSelectorHighlight(vtkRenderer& renderer, vtkActor& highlight_actor,
    GeometryActorSelectOp select_op, vtkSmartPointer<IVtkTools_ShapePicker> picker)
    : renderer_(&renderer)
    , highlight_actor_(&highlight_actor)
    , select_op_(std::move(select_op))
    , picker_(std::move(picker))
{
    hl_ = select_op_.buildHighlight(SelectMode::GeometryEdge);
    mountHighlight(highlight_actor_, hl_, SelectMode::GeometryEdge);
    select_op_.configurePicker(picker_, SelectMode::GeometryEdge);
}

GeometryEdgeSelectorHighlight::~GeometryEdgeSelectorHighlight()
{
    clear();
}

void GeometryEdgeSelectorHighlight::clear()
{
    selections_.clear();
    if (hl_.filter) {
        hl_.filter->Clear();
        hl_.filter->Modified();
    }
    if (renderer_ && renderer_->GetRenderWindow())
        renderer_->GetRenderWindow()->Render();
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

    if (hl_.filter)
        hl_.filter->SetData(ids);
    flushHighlight(hl_.filter, renderer_);
}

// ─── Vertex ────────────────────────────────────────────

GeometryVertexSelectorHighlight::GeometryVertexSelectorHighlight(vtkRenderer& renderer, vtkActor& highlight_actor,
    GeometryActorSelectOp select_op, vtkSmartPointer<IVtkTools_ShapePicker> picker)
    : renderer_(&renderer)
    , highlight_actor_(&highlight_actor)
    , select_op_(std::move(select_op))
    , picker_(std::move(picker))
{
    hl_ = select_op_.buildHighlight(SelectMode::GeometryVertex);
    mountHighlight(highlight_actor_, hl_, SelectMode::GeometryVertex);
    select_op_.configurePicker(picker_, SelectMode::GeometryVertex);
}

GeometryVertexSelectorHighlight::~GeometryVertexSelectorHighlight()
{
    clear();
}

void GeometryVertexSelectorHighlight::clear()
{
    selections_.clear();
    if (hl_.filter) {
        hl_.filter->Clear();
        hl_.filter->Modified();
    }
    if (renderer_ && renderer_->GetRenderWindow())
        renderer_->GetRenderWindow()->Render();
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

    if (hl_.filter)
        hl_.filter->SetData(ids);
    flushHighlight(hl_.filter, renderer_);
}

// ─── Solid ─────────────────────────────────────────────

GeometrySolidSelectorHighlight::GeometrySolidSelectorHighlight(vtkRenderer& renderer, vtkActor& highlight_actor,
    GeometryActorSelectOp select_op, vtkSmartPointer<IVtkTools_ShapePicker> picker)
    : renderer_(&renderer)
    , highlight_actor_(&highlight_actor)
    , select_op_(std::move(select_op))
    , picker_(std::move(picker))
{
    hl_ = select_op_.buildHighlight(SelectMode::GeometrySolid);
    mountHighlight(highlight_actor_, hl_, SelectMode::GeometrySolid);
    select_op_.configurePicker(picker_, SelectMode::GeometrySolid);
}

GeometrySolidSelectorHighlight::~GeometrySolidSelectorHighlight()
{
    clear();
}

void GeometrySolidSelectorHighlight::clear()
{
    selections_.clear();
    highlighted_face_ids_.clear();
    if (hl_.filter) {
        hl_.filter->Clear();
        hl_.filter->Modified();
    }
    if (renderer_ && renderer_->GetRenderWindow())
        renderer_->GetRenderWindow()->Render();
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

    if (hl_.filter)
        hl_.filter->SetData(ids);
    flushHighlight(hl_.filter, renderer_);
}
