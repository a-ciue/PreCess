#include "GeometrySelectorHighlight.h"
#include "GeometryActor.h"
#include "GeometrySubshapeIndex.h"
#include "Core.h"

#include <IVtkOCC_Shape.hxx>
#include <IVtkTools_ShapePicker.hxx>
#include <IVtkTools_SubPolyDataFilter.hxx>
#include <IVtk_Types.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>

#include <vtkActor.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>

static constexpr IVtk_SelectionMode kSelVertex = SM_Vertex;
static constexpr IVtk_SelectionMode kSelEdge   = SM_Edge;
static constexpr IVtk_SelectionMode kSelFace   = SM_Face;
static constexpr IVtk_SelectionMode kSelSolid  = SM_Shape;

static int toleranceForMode(const SelectMode m)
{
    if (m == SelectMode::Vertex)
        return 40;
    if (m == SelectMode::Edge)
        return 25;
    if (m == SelectMode::Solid)
        return 8;
    return 8;
}

static bool firstId(const NCollection_List<IVtk_IdType>& list, IVtk_IdType& outId)
{
    if (list.IsEmpty())
        return false;
    outId = list.First();
    return true;
}

static std::optional<Index> mapSubshapeToGeomId(const OccShapeHandle& occShape,
    const GeometrySubshapeIndex& geomIndex,
    TopAbs_ShapeEnum wantType,
    IVtk_IdType subId,
    ElementEnum::Type& outType)
{
    if (occShape.IsNull())
        return std::nullopt;

    const TopoDS_Shape& sub = occShape->GetSubShape(subId);
    if (sub.IsNull())
        return std::nullopt;

    const int ti = GeometrySubshapeIndex::typeIndex(wantType);
    if (ti < 0)
        return std::nullopt;

    const int localTypeId = geomIndex.type_maps[static_cast<size_t>(ti)].FindIndex(sub);
    if (localTypeId <= 0)
        return std::nullopt;

    if (wantType == TopAbs_FACE) {
        GeomFaceId gid = geomIndex.faceGlobalId(localTypeId);
        if (gid == kInvalidGeomFaceId)
            return std::nullopt;
        outType = ElementEnum::Face;
        return gid;
    } else if (wantType == TopAbs_EDGE) {
        GeomEdgeId gid = geomIndex.edgeGlobalId(localTypeId);
        if (gid == kInvalidGeomEdgeId)
            return std::nullopt;
        outType = ElementEnum::Edge;
        return gid;
    } else if (wantType == TopAbs_VERTEX) {
        GeomVertexId gid = geomIndex.vertexGlobalId(localTypeId);
        if (gid == kInvalidGeomVertexId)
            return std::nullopt;
        outType = ElementEnum::Vertex;
        return gid;
    } else if (wantType == TopAbs_SOLID) {
        GeomSolidId gid = geomIndex.solidGlobalId(localTypeId);
        if (gid == kInvalidGeomSolidId)
            return std::nullopt;
        outType = ElementEnum::Solid;
        return gid;
    }
    return std::nullopt;
}

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

    const int n = picker_->Pick(posx, posy, 0.0, renderer_);
    if (n <= 0)
        return;

    IVtk_IdType shapeId = -1;
    if (!firstId(picker_->GetPickedShapesIds(false), shapeId))
        return;

    IVtk_IdType subId = -1;
    if (!firstId(picker_->GetPickedSubShapesIds(shapeId, false), subId))
        return;

    const auto* geomIndex = op->getGeometryIndex();
    if (!geomIndex)
        return;

    ElementEnum::Type elemType = ElementEnum::None;
    auto geomId = mapSubshapeToGeomId(op->getOccShape(), *geomIndex, TopAbs_FACE, subId, elemType);
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
    {
        auto oldOp = geom_actor_.lock();
        if (oldOp) {
            picker_->SetSelectionMode(oldOp->getOccShape(), kSelFace, false);
            picker_->SetSelectionMode(oldOp->getOccShape(), kSelEdge, false);
            picker_->SetSelectionMode(oldOp->getOccShape(), kSelVertex, false);
            auto& poly = dynamic_cast<vtkActor&>(oldOp->getPolyActor());
            picker_->SetSelectionMode(&poly, kSelFace, false);
            picker_->SetSelectionMode(&poly, kSelEdge, false);
            picker_->SetSelectionMode(&poly, kSelVertex, false);
            auto& line = dynamic_cast<vtkActor&>(oldOp->getLineActor());
            picker_->SetSelectionMode(&line, kSelFace, false);
            picker_->SetSelectionMode(&line, kSelEdge, false);
            picker_->SetSelectionMode(&line, kSelVertex, false);
            picker_->InitializePickList();
        }
    }

    clear();
    geom_actor_ = std::move(geom_actor);
    filter_ = nullptr;
    hl_actor_ = nullptr;

    auto op = geom_actor_.lock();
    if (op) {
        filter_ = &op->getPolyHLFilter();
        hl_actor_ = &op->getPolyHLActor();
    }

    configurePicker();
}

void GeometryFaceSelectorHighlight::configurePicker()
{
    auto op = geom_actor_.lock();
    if (!op)
        return;

    picker_->PickFromListOn();
    picker_->InitializePickList();
    picker_->AddPickList(&dynamic_cast<vtkActor&>(op->getPolyActor()));
    picker_->SetPixelTolerance(toleranceForMode(SelectMode::Face));

    picker_->SetSelectionMode(op->getOccShape(), kSelFace, false);
    picker_->SetSelectionMode(op->getOccShape(), kSelEdge, false);
    picker_->SetSelectionMode(op->getOccShape(), kSelVertex, false);
    picker_->SetSelectionMode(op->getOccShape(), kSelFace, true);

    auto& poly = dynamic_cast<vtkActor&>(op->getPolyActor());
    picker_->SetSelectionMode(&poly, kSelFace, true);
    picker_->SetSelectionMode(&poly, kSelEdge, false);
    picker_->SetSelectionMode(&poly, kSelVertex, false);

    auto& line = dynamic_cast<vtkActor&>(op->getLineActor());
    picker_->SetSelectionMode(&line, kSelFace, false);
    picker_->SetSelectionMode(&line, kSelEdge, false);
    picker_->SetSelectionMode(&line, kSelVertex, false);
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

    const int n = picker_->Pick(posx, posy, 0.0, renderer_);
    if (n <= 0)
        return;

    IVtk_IdType shapeId = -1;
    if (!firstId(picker_->GetPickedShapesIds(false), shapeId))
        return;

    IVtk_IdType subId = -1;
    if (!firstId(picker_->GetPickedSubShapesIds(shapeId, false), subId))
        return;

    const auto* geomIndex = op->getGeometryIndex();
    if (!geomIndex)
        return;

    ElementEnum::Type elemType = ElementEnum::None;
    auto geomId = mapSubshapeToGeomId(op->getOccShape(), *geomIndex, TopAbs_EDGE, subId, elemType);
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
    {
        auto oldOp = geom_actor_.lock();
        if (oldOp) {
            picker_->SetSelectionMode(oldOp->getOccShape(), kSelFace, false);
            picker_->SetSelectionMode(oldOp->getOccShape(), kSelEdge, false);
            picker_->SetSelectionMode(oldOp->getOccShape(), kSelVertex, false);
            auto& poly = dynamic_cast<vtkActor&>(oldOp->getPolyActor());
            picker_->SetSelectionMode(&poly, kSelFace, false);
            picker_->SetSelectionMode(&poly, kSelEdge, false);
            picker_->SetSelectionMode(&poly, kSelVertex, false);
            auto& line = dynamic_cast<vtkActor&>(oldOp->getLineActor());
            picker_->SetSelectionMode(&line, kSelFace, false);
            picker_->SetSelectionMode(&line, kSelEdge, false);
            picker_->SetSelectionMode(&line, kSelVertex, false);
            picker_->InitializePickList();
        }
    }

    clear();
    geom_actor_ = std::move(geom_actor);
    filter_ = nullptr;
    hl_actor_ = nullptr;

    auto op = geom_actor_.lock();
    if (op) {
        filter_ = &op->getLineHLFilter();
        hl_actor_ = &op->getLineHLActor();
    }

    configurePicker();
}

void GeometryEdgeSelectorHighlight::configurePicker()
{
    auto op = geom_actor_.lock();
    if (!op)
        return;

    picker_->PickFromListOn();
    picker_->InitializePickList();
    picker_->AddPickList(&dynamic_cast<vtkActor&>(op->getLineActor()));
    {
        auto& poly = dynamic_cast<vtkActor&>(op->getPolyActor());
        picker_->AddPickList(&poly);
    }
    picker_->SetPixelTolerance(toleranceForMode(SelectMode::Edge));

    picker_->SetSelectionMode(op->getOccShape(), kSelFace, false);
    picker_->SetSelectionMode(op->getOccShape(), kSelEdge, false);
    picker_->SetSelectionMode(op->getOccShape(), kSelVertex, false);
    picker_->SetSelectionMode(op->getOccShape(), kSelEdge, true);

    {
        auto& poly = dynamic_cast<vtkActor&>(op->getPolyActor());
        picker_->SetSelectionMode(&poly, kSelFace, false);
        picker_->SetSelectionMode(&poly, kSelEdge, false);
        picker_->SetSelectionMode(&poly, kSelVertex, false);
    }

    auto& line = dynamic_cast<vtkActor&>(op->getLineActor());
    picker_->SetSelectionMode(&line, kSelEdge, true);
    picker_->SetSelectionMode(&line, kSelVertex, false);
    picker_->SetSelectionMode(&line, kSelFace, false);
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

    const int n = picker_->Pick(posx, posy, 0.0, renderer_);
    if (n <= 0)
        return;

    IVtk_IdType shapeId = -1;
    if (!firstId(picker_->GetPickedShapesIds(false), shapeId))
        return;

    IVtk_IdType subId = -1;
    if (!firstId(picker_->GetPickedSubShapesIds(shapeId, false), subId))
        return;

    const auto* geomIndex = op->getGeometryIndex();
    if (!geomIndex)
        return;

    ElementEnum::Type elemType = ElementEnum::None;
    auto geomId = mapSubshapeToGeomId(op->getOccShape(), *geomIndex, TopAbs_VERTEX, subId, elemType);
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
    {
        auto oldOp = geom_actor_.lock();
        if (oldOp) {
            picker_->SetSelectionMode(oldOp->getOccShape(), kSelFace, false);
            picker_->SetSelectionMode(oldOp->getOccShape(), kSelEdge, false);
            picker_->SetSelectionMode(oldOp->getOccShape(), kSelVertex, false);
            auto& poly = dynamic_cast<vtkActor&>(oldOp->getPolyActor());
            picker_->SetSelectionMode(&poly, kSelFace, false);
            picker_->SetSelectionMode(&poly, kSelEdge, false);
            picker_->SetSelectionMode(&poly, kSelVertex, false);
            auto& line = dynamic_cast<vtkActor&>(oldOp->getLineActor());
            picker_->SetSelectionMode(&line, kSelFace, false);
            picker_->SetSelectionMode(&line, kSelEdge, false);
            picker_->SetSelectionMode(&line, kSelVertex, false);
            picker_->InitializePickList();
        }
    }

    clear();
    geom_actor_ = std::move(geom_actor);
    filter_ = nullptr;
    hl_actor_ = nullptr;

    auto op = geom_actor_.lock();
    if (op) {
        filter_ = &op->getLineHLFilter();
        hl_actor_ = &op->getLineHLActor();
    }

    configurePicker();
}

void GeometryVertexSelectorHighlight::configurePicker()
{
    auto op = geom_actor_.lock();
    if (!op)
        return;

    picker_->PickFromListOn();
    picker_->InitializePickList();
    picker_->AddPickList(&dynamic_cast<vtkActor&>(op->getLineActor()));
    {
        auto& poly = dynamic_cast<vtkActor&>(op->getPolyActor());
        picker_->AddPickList(&poly);
    }
    picker_->SetPixelTolerance(toleranceForMode(SelectMode::Vertex));

    picker_->SetSelectionMode(op->getOccShape(), kSelFace, false);
    picker_->SetSelectionMode(op->getOccShape(), kSelEdge, false);
    picker_->SetSelectionMode(op->getOccShape(), kSelVertex, false);
    picker_->SetSelectionMode(op->getOccShape(), kSelVertex, true);

    {
        auto& poly = dynamic_cast<vtkActor&>(op->getPolyActor());
        picker_->SetSelectionMode(&poly, kSelFace, false);
        picker_->SetSelectionMode(&poly, kSelEdge, false);
        picker_->SetSelectionMode(&poly, kSelVertex, false);
    }

    auto& line = dynamic_cast<vtkActor&>(op->getLineActor());
    picker_->SetSelectionMode(&line, kSelVertex, true);
    picker_->SetSelectionMode(&line, kSelEdge, false);
    picker_->SetSelectionMode(&line, kSelFace, false);
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

    const int n = picker_->Pick(posx, posy, 0.0, renderer_);
    if (n <= 0)
        return;

    IVtk_IdType shapeId = -1;
    if (!firstId(picker_->GetPickedShapesIds(false), shapeId))
        return;

    IVtk_IdType subId = -1;
    if (!firstId(picker_->GetPickedSubShapesIds(shapeId, false), subId))
        return;

    const TopoDS_Shape& pickedSub = op->getOccShape()->GetSubShape(subId);
    if (pickedSub.IsNull())
        return;

    const auto* geomIndex = op->getGeometryIndex();
    if (!geomIndex)
        return;

    TopoDS_Shape solidShape;
    if (pickedSub.ShapeType() == TopAbs_SOLID) {
        solidShape = pickedSub;
    } else {
        const int solidTi = GeometrySubshapeIndex::typeIndex(TopAbs_SOLID);
        const int nSolids = geomIndex->type_maps[solidTi].Extent();
        for (int localId = 1; localId <= nSolids; ++localId) {
            const TopoDS_Shape& solid = geomIndex->type_maps[solidTi].FindKey(localId);
            for (TopExp_Explorer exp(solid, TopAbs_FACE); exp.More(); exp.Next()) {
                if (exp.Current().IsSame(pickedSub)) {
                    solidShape = solid;
                    break;
                }
            }
            if (!solidShape.IsNull())
                break;
        }
        if (solidShape.IsNull())
            return;
    }

    const int solidTi = GeometrySubshapeIndex::typeIndex(TopAbs_SOLID);
    const int localTypeId = geomIndex->type_maps[solidTi].FindIndex(solidShape);
    if (localTypeId <= 0)
        return;

    GeomSolidId gid = geomIndex->solidGlobalId(localTypeId);
    if (gid == kInvalidGeomSolidId)
        return;

    auto [it, inserted] = selections_.insert_or_assign(gid, gid);
    if (!inserted) {
        selections_.erase(it);
    }

    for (TopExp_Explorer expF(solidShape, TopAbs_FACE); expF.More(); expF.Next()) {
        const TopoDS_Shape& face = expF.Current();
        IVtk_IdType faceSubId = op->getOccShape()->GetSubShapeId(face);
        if (faceSubId >= 0) {
            if (!inserted)
                highlighted_face_ids_.erase(faceSubId);
            else
                highlighted_face_ids_.insert(faceSubId);
        }
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
    {
        auto oldOp = geom_actor_.lock();
        if (oldOp) {
            picker_->SetSelectionMode(oldOp->getOccShape(), kSelFace, false);
            picker_->SetSelectionMode(oldOp->getOccShape(), kSelEdge, false);
            picker_->SetSelectionMode(oldOp->getOccShape(), kSelVertex, false);
            auto& poly = dynamic_cast<vtkActor&>(oldOp->getPolyActor());
            picker_->SetSelectionMode(&poly, kSelFace, false);
            picker_->SetSelectionMode(&poly, kSelEdge, false);
            picker_->SetSelectionMode(&poly, kSelVertex, false);
            auto& line = dynamic_cast<vtkActor&>(oldOp->getLineActor());
            picker_->SetSelectionMode(&line, kSelFace, false);
            picker_->SetSelectionMode(&line, kSelEdge, false);
            picker_->SetSelectionMode(&line, kSelVertex, false);
            picker_->InitializePickList();
        }
    }

    clear();
    geom_actor_ = std::move(geom_actor);
    filter_ = nullptr;
    hl_actor_ = nullptr;

    auto op = geom_actor_.lock();
    if (op) {
        filter_ = &op->getPolyHLFilter();
        hl_actor_ = &op->getPolyHLActor();
    }

    configurePicker();
}

void GeometrySolidSelectorHighlight::configurePicker()
{
    auto op = geom_actor_.lock();
    if (!op) {
        if (picker_)
            picker_->InitializePickList();
        return;
    }

    picker_->PickFromListOn();
    picker_->InitializePickList();
    picker_->AddPickList(&dynamic_cast<vtkActor&>(op->getPolyActor()));
    picker_->SetPixelTolerance(toleranceForMode(SelectMode::Solid));

    picker_->SetSelectionMode(op->getOccShape(), kSelFace, false);
    picker_->SetSelectionMode(op->getOccShape(), kSelEdge, false);
    picker_->SetSelectionMode(op->getOccShape(), kSelVertex, false);
    picker_->SetSelectionMode(op->getOccShape(), kSelFace, true);

    auto& poly = dynamic_cast<vtkActor&>(op->getPolyActor());
    picker_->SetSelectionMode(&poly, kSelFace, true);
    picker_->SetSelectionMode(&poly, kSelEdge, false);
    picker_->SetSelectionMode(&poly, kSelVertex, false);

    auto& line = dynamic_cast<vtkActor&>(op->getLineActor());
    picker_->SetSelectionMode(&line, kSelFace, false);
    picker_->SetSelectionMode(&line, kSelEdge, false);
    picker_->SetSelectionMode(&line, kSelVertex, false);
}
