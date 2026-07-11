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

static void flushHighlight(IVtkTools_SubPolyDataFilter* filter, vtkActor* hlActor, bool visible, vtkRenderer* renderer)
{
    if (!filter || !hlActor || !renderer)
        return;
    filter->Modified();
    hlActor->SetVisibility(visible);
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
    if (mode == SelectMode::Edge || mode == SelectMode::Vertex) {
        prop->SetOpacity(0.5);
        prop->RenderLinesAsTubesOn();
        prop->SetLineWidth(3.0);
        prop->SetPointSize(8.0);
        prop->LightingOff();
    } else {
        prop->SetOpacity(1.0);
    }
    actor->SetProperty(prop);
    actor->SetVisibility(false);
}

// ─── Face ──────────────────────────────────────────────

GeometryFaceSelectorHighlight::GeometryFaceSelectorHighlight(vtkRenderer* renderer, vtkActor* highlight_actor)
    : renderer_(renderer)
    , highlight_actor_(highlight_actor)
{
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
    if (highlight_actor_)
        highlight_actor_->SetVisibility(false);
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
    auto op = geom_actor_.lock();
    if (!op)
        return;

    IVtk_IdType subId = -1;
    auto geomId = op->pickSubshape(picker_, renderer_, posx, posy, SelectMode::Face, subId);
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
    flushHighlight(hl_.filter, highlight_actor_, !ids.IsEmpty(), renderer_);
}

void GeometryFaceSelectorHighlight::setCurGeomActor(GeometryActorSelectOpFactory geom_actor)
{
    if (auto oldOp = geom_actor_.lock())
        oldOp->disablePickerModes(picker_);

    clear();
    hl_ = {};

    geom_actor_ = std::move(geom_actor);

    auto op = geom_actor_.lock();
    if (op) {
        hl_ = op->buildHighlight(SelectMode::Face);
        mountHighlight(highlight_actor_, hl_, SelectMode::Face);
        op->configurePicker(picker_, SelectMode::Face);
    }
}

// ─── Edge ──────────────────────────────────────────────

GeometryEdgeSelectorHighlight::GeometryEdgeSelectorHighlight(vtkRenderer* renderer, vtkActor* highlight_actor)
    : renderer_(renderer)
    , highlight_actor_(highlight_actor)
{
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
    if (highlight_actor_)
        highlight_actor_->SetVisibility(false);
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
    auto op = geom_actor_.lock();
    if (!op)
        return;

    IVtk_IdType subId = -1;
    auto geomId = op->pickSubshape(picker_, renderer_, posx, posy, SelectMode::Edge, subId);
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
    flushHighlight(hl_.filter, highlight_actor_, !ids.IsEmpty(), renderer_);
}

void GeometryEdgeSelectorHighlight::setCurGeomActor(GeometryActorSelectOpFactory geom_actor)
{
    if (auto oldOp = geom_actor_.lock())
        oldOp->disablePickerModes(picker_);

    clear();
    hl_ = {};

    geom_actor_ = std::move(geom_actor);

    auto op = geom_actor_.lock();
    if (op) {
        hl_ = op->buildHighlight(SelectMode::Edge);
        mountHighlight(highlight_actor_, hl_, SelectMode::Edge);
        op->configurePicker(picker_, SelectMode::Edge);
    }
}

// ─── Vertex ────────────────────────────────────────────

GeometryVertexSelectorHighlight::GeometryVertexSelectorHighlight(vtkRenderer* renderer, vtkActor* highlight_actor)
    : renderer_(renderer)
    , highlight_actor_(highlight_actor)
{
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
    if (highlight_actor_)
        highlight_actor_->SetVisibility(false);
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
    auto op = geom_actor_.lock();
    if (!op)
        return;

    IVtk_IdType subId = -1;
    auto geomId = op->pickSubshape(picker_, renderer_, posx, posy, SelectMode::Vertex, subId);
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
    flushHighlight(hl_.filter, highlight_actor_, !ids.IsEmpty(), renderer_);
}

void GeometryVertexSelectorHighlight::setCurGeomActor(GeometryActorSelectOpFactory geom_actor)
{
    if (auto oldOp = geom_actor_.lock())
        oldOp->disablePickerModes(picker_);

    clear();
    hl_ = {};

    geom_actor_ = std::move(geom_actor);

    auto op = geom_actor_.lock();
    if (op) {
        hl_ = op->buildHighlight(SelectMode::Vertex);
        mountHighlight(highlight_actor_, hl_, SelectMode::Vertex);
        op->configurePicker(picker_, SelectMode::Vertex);
    }
}

// ─── Solid ─────────────────────────────────────────────

GeometrySolidSelectorHighlight::GeometrySolidSelectorHighlight(vtkRenderer* renderer, vtkActor* highlight_actor)
    : renderer_(renderer)
    , highlight_actor_(highlight_actor)
{
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
    if (highlight_actor_)
        highlight_actor_->SetVisibility(false);
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
    auto op = geom_actor_.lock();
    if (!op)
        return;

    GeomSolidId gid = kInvalidGeomSolidId;
    std::vector<IVtk_IdType> faceSubIds;
    if (!op->pickSolid(picker_, renderer_, posx, posy, gid, faceSubIds))
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
    flushHighlight(hl_.filter, highlight_actor_, !ids.IsEmpty(), renderer_);
}

void GeometrySolidSelectorHighlight::setCurGeomActor(GeometryActorSelectOpFactory geom_actor)
{
    if (auto oldOp = geom_actor_.lock())
        oldOp->disablePickerModes(picker_);

    clear();
    hl_ = {};

    geom_actor_ = std::move(geom_actor);

    auto op = geom_actor_.lock();
    if (op) {
        hl_ = op->buildHighlight(SelectMode::Solid);
        mountHighlight(highlight_actor_, hl_, SelectMode::Solid);
        op->configurePicker(picker_, SelectMode::Solid);
    }
}
