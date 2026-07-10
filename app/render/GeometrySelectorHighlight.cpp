#include "GeometrySelectorHighlight.h"
#include "GeometryActorSelectOp.h"
#include "Core.h"

#include <IVtkTools_ShapePicker.hxx>
#include <IVtkTools_SubPolyDataFilter.hxx>
#include <IVtk_Types.hxx>
#include <NCollection_List.hxx>
#include <vtkActor.h>
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

static void removeHighlightActor(GeometryHighlightPipeline& hl, vtkRenderer* renderer)
{
    if (hl.actor && renderer) {
        renderer->RemoveActor(hl.actor);
        if (renderer->GetRenderWindow())
            renderer->GetRenderWindow()->Render();
    }
}

// ─── Face ──────────────────────────────────────────────

GeometryFaceSelectorHighlight::GeometryFaceSelectorHighlight(vtkRenderer* renderer)
    : renderer_(renderer)
{
}

GeometryFaceSelectorHighlight::~GeometryFaceSelectorHighlight()
{
    clear();
    removeHighlightActor(hl_, renderer_);
}

void GeometryFaceSelectorHighlight::clear()
{
    selections_.clear();
    if (hl_.filter) {
        hl_.filter->Clear();
        hl_.filter->Modified();
    }
    if (hl_.actor)
        hl_.actor->SetVisibility(false);
    if (renderer_ && renderer_->GetRenderWindow())
        renderer_->GetRenderWindow()->Render();
}

GeometrySelectionVtk GeometryFaceSelectorHighlight::get() const
{
    GeometrySelectionVtk s;
    s.type = ElementEnum::Face;
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
    flushHighlight(hl_.filter, hl_.actor, !ids.IsEmpty(), renderer_);
}

void GeometryFaceSelectorHighlight::setCurGeomActor(GeometryActorSelectOpFactory geom_actor)
{
    if (auto oldOp = geom_actor_.lock())
        oldOp->disablePickerModes(picker_);

    clear();
    removeHighlightActor(hl_, renderer_);
    hl_ = {};

    geom_actor_ = std::move(geom_actor);

    auto op = geom_actor_.lock();
    if (op) {
        hl_ = op->buildHighlight(SelectMode::Face);
        if (renderer_ && hl_.actor)
            renderer_->AddActor(hl_.actor);
        op->configurePicker(picker_, SelectMode::Face);
    }
}

// ─── Edge ──────────────────────────────────────────────

GeometryEdgeSelectorHighlight::GeometryEdgeSelectorHighlight(vtkRenderer* renderer)
    : renderer_(renderer)
{
}

GeometryEdgeSelectorHighlight::~GeometryEdgeSelectorHighlight()
{
    clear();
    removeHighlightActor(hl_, renderer_);
}

void GeometryEdgeSelectorHighlight::clear()
{
    selections_.clear();
    if (hl_.filter) {
        hl_.filter->Clear();
        hl_.filter->Modified();
    }
    if (hl_.actor)
        hl_.actor->SetVisibility(false);
    if (renderer_ && renderer_->GetRenderWindow())
        renderer_->GetRenderWindow()->Render();
}

GeometrySelectionVtk GeometryEdgeSelectorHighlight::get() const
{
    GeometrySelectionVtk s;
    s.type = ElementEnum::Edge;
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
    flushHighlight(hl_.filter, hl_.actor, !ids.IsEmpty(), renderer_);
}

void GeometryEdgeSelectorHighlight::setCurGeomActor(GeometryActorSelectOpFactory geom_actor)
{
    if (auto oldOp = geom_actor_.lock())
        oldOp->disablePickerModes(picker_);

    clear();
    removeHighlightActor(hl_, renderer_);
    hl_ = {};

    geom_actor_ = std::move(geom_actor);

    auto op = geom_actor_.lock();
    if (op) {
        hl_ = op->buildHighlight(SelectMode::Edge);
        if (renderer_ && hl_.actor)
            renderer_->AddActor(hl_.actor);
        op->configurePicker(picker_, SelectMode::Edge);
    }
}

// ─── Vertex ────────────────────────────────────────────

GeometryVertexSelectorHighlight::GeometryVertexSelectorHighlight(vtkRenderer* renderer)
    : renderer_(renderer)
{
}

GeometryVertexSelectorHighlight::~GeometryVertexSelectorHighlight()
{
    clear();
    removeHighlightActor(hl_, renderer_);
}

void GeometryVertexSelectorHighlight::clear()
{
    selections_.clear();
    if (hl_.filter) {
        hl_.filter->Clear();
        hl_.filter->Modified();
    }
    if (hl_.actor)
        hl_.actor->SetVisibility(false);
    if (renderer_ && renderer_->GetRenderWindow())
        renderer_->GetRenderWindow()->Render();
}

GeometrySelectionVtk GeometryVertexSelectorHighlight::get() const
{
    GeometrySelectionVtk s;
    s.type = ElementEnum::Vertex;
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
    flushHighlight(hl_.filter, hl_.actor, !ids.IsEmpty(), renderer_);
}

void GeometryVertexSelectorHighlight::setCurGeomActor(GeometryActorSelectOpFactory geom_actor)
{
    if (auto oldOp = geom_actor_.lock())
        oldOp->disablePickerModes(picker_);

    clear();
    removeHighlightActor(hl_, renderer_);
    hl_ = {};

    geom_actor_ = std::move(geom_actor);

    auto op = geom_actor_.lock();
    if (op) {
        hl_ = op->buildHighlight(SelectMode::Vertex);
        if (renderer_ && hl_.actor)
            renderer_->AddActor(hl_.actor);
        op->configurePicker(picker_, SelectMode::Vertex);
    }
}

// ─── Solid ─────────────────────────────────────────────

GeometrySolidSelectorHighlight::GeometrySolidSelectorHighlight(vtkRenderer* renderer)
    : renderer_(renderer)
{
}

GeometrySolidSelectorHighlight::~GeometrySolidSelectorHighlight()
{
    clear();
    removeHighlightActor(hl_, renderer_);
}

void GeometrySolidSelectorHighlight::clear()
{
    selections_.clear();
    highlighted_face_ids_.clear();
    if (hl_.filter) {
        hl_.filter->Clear();
        hl_.filter->Modified();
    }
    if (hl_.actor)
        hl_.actor->SetVisibility(false);
    if (renderer_ && renderer_->GetRenderWindow())
        renderer_->GetRenderWindow()->Render();
}

GeometrySelectionVtk GeometrySolidSelectorHighlight::get() const
{
    GeometrySelectionVtk s;
    s.type = ElementEnum::Solid;
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
    flushHighlight(hl_.filter, hl_.actor, !ids.IsEmpty(), renderer_);
}

void GeometrySolidSelectorHighlight::setCurGeomActor(GeometryActorSelectOpFactory geom_actor)
{
    if (auto oldOp = geom_actor_.lock())
        oldOp->disablePickerModes(picker_);

    clear();
    removeHighlightActor(hl_, renderer_);
    hl_ = {};

    geom_actor_ = std::move(geom_actor);

    auto op = geom_actor_.lock();
    if (op) {
        hl_ = op->buildHighlight(SelectMode::Solid);
        if (renderer_ && hl_.actor)
            renderer_->AddActor(hl_.actor);
        op->configurePicker(picker_, SelectMode::Solid);
    }
}
