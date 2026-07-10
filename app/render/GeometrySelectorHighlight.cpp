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

// ─── Face ──────────────────────────────────────────────

GeometryFaceSelectorHighlight::GeometryFaceSelectorHighlight(vtkRenderer* renderer)
    : renderer_(renderer)
{
}

GeometryFaceSelectorHighlight::~GeometryFaceSelectorHighlight() { clear(); }

void GeometryFaceSelectorHighlight::clear()
{
    selections_.clear();
    if (filter_) {
        filter_->Clear();
        filter_->Modified();
    }
    if (hl_actor_)
        hl_actor_->SetVisibility(false);
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

    if (filter_)
        filter_->SetData(ids);
    flushHighlight(filter_, hl_actor_, !ids.IsEmpty(), renderer_);
}

void GeometryFaceSelectorHighlight::setCurGeomActor(GeometryActorSelectOpFactory geom_actor)
{
    if (auto oldOp = geom_actor_.lock())
        oldOp->disablePickerModes(picker_);

    clear();
    geom_actor_ = std::move(geom_actor);
    filter_ = nullptr;
    hl_actor_ = nullptr;

    auto op = geom_actor_.lock();
    if (op) {
        filter_ = op->highlightFilter(SelectMode::Face);
        hl_actor_ = op->highlightActor(SelectMode::Face);
        op->configurePicker(picker_, SelectMode::Face);
    }
}

// ─── Edge ──────────────────────────────────────────────

GeometryEdgeSelectorHighlight::GeometryEdgeSelectorHighlight(vtkRenderer* renderer)
    : renderer_(renderer)
{
}

GeometryEdgeSelectorHighlight::~GeometryEdgeSelectorHighlight() { clear(); }

void GeometryEdgeSelectorHighlight::clear()
{
    selections_.clear();
    if (filter_) {
        filter_->Clear();
        filter_->Modified();
    }
    if (hl_actor_)
        hl_actor_->SetVisibility(false);
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

    if (filter_)
        filter_->SetData(ids);
    flushHighlight(filter_, hl_actor_, !ids.IsEmpty(), renderer_);
}

void GeometryEdgeSelectorHighlight::setCurGeomActor(GeometryActorSelectOpFactory geom_actor)
{
    if (auto oldOp = geom_actor_.lock())
        oldOp->disablePickerModes(picker_);

    clear();
    geom_actor_ = std::move(geom_actor);
    filter_ = nullptr;
    hl_actor_ = nullptr;

    auto op = geom_actor_.lock();
    if (op) {
        filter_ = op->highlightFilter(SelectMode::Edge);
        hl_actor_ = op->highlightActor(SelectMode::Edge);
        op->configurePicker(picker_, SelectMode::Edge);
    }
}

// ─── Vertex ────────────────────────────────────────────

GeometryVertexSelectorHighlight::GeometryVertexSelectorHighlight(vtkRenderer* renderer)
    : renderer_(renderer)
{
}

GeometryVertexSelectorHighlight::~GeometryVertexSelectorHighlight() { clear(); }

void GeometryVertexSelectorHighlight::clear()
{
    selections_.clear();
    if (filter_) {
        filter_->Clear();
        filter_->Modified();
    }
    if (hl_actor_)
        hl_actor_->SetVisibility(false);
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

    if (filter_)
        filter_->SetData(ids);
    flushHighlight(filter_, hl_actor_, !ids.IsEmpty(), renderer_);
}

void GeometryVertexSelectorHighlight::setCurGeomActor(GeometryActorSelectOpFactory geom_actor)
{
    if (auto oldOp = geom_actor_.lock())
        oldOp->disablePickerModes(picker_);

    clear();
    geom_actor_ = std::move(geom_actor);
    filter_ = nullptr;
    hl_actor_ = nullptr;

    auto op = geom_actor_.lock();
    if (op) {
        filter_ = op->highlightFilter(SelectMode::Vertex);
        hl_actor_ = op->highlightActor(SelectMode::Vertex);
        op->configurePicker(picker_, SelectMode::Vertex);
    }
}

// ─── Solid ─────────────────────────────────────────────

GeometrySolidSelectorHighlight::GeometrySolidSelectorHighlight(vtkRenderer* renderer)
    : renderer_(renderer)
{
}

GeometrySolidSelectorHighlight::~GeometrySolidSelectorHighlight() { clear(); }

void GeometrySolidSelectorHighlight::clear()
{
    selections_.clear();
    highlighted_face_ids_.clear();
    if (filter_) {
        filter_->Clear();
        filter_->Modified();
    }
    if (hl_actor_)
        hl_actor_->SetVisibility(false);
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

    if (filter_)
        filter_->SetData(ids);
    flushHighlight(filter_, hl_actor_, !ids.IsEmpty(), renderer_);
}

void GeometrySolidSelectorHighlight::setCurGeomActor(GeometryActorSelectOpFactory geom_actor)
{
    if (auto oldOp = geom_actor_.lock())
        oldOp->disablePickerModes(picker_);

    clear();
    geom_actor_ = std::move(geom_actor);
    filter_ = nullptr;
    hl_actor_ = nullptr;

    auto op = geom_actor_.lock();
    if (op) {
        filter_ = op->highlightFilter(SelectMode::Solid);
        hl_actor_ = op->highlightActor(SelectMode::Solid);
        op->configurePicker(picker_, SelectMode::Solid);
    }
}
