#include "GeometryActorSelectOp.h"
#include "Core.h"
#include "GeometryActor.h"
#include "GeometrySubshapeIndex.h"
#include "Selection.h"

#include <IVtkOCC_Shape.hxx>
#include <IVtkTools_ShapePicker.hxx>
#include <IVtkTools_SubPolyDataFilter.hxx>
#include <IVtk_Types.hxx>
#include <NCollection_List.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>
#include <vtkActor.h>
#include <vtkDataArray.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>

static constexpr IVtk_SelectionMode kSelVertex = SM_Vertex;
static constexpr IVtk_SelectionMode kSelEdge = SM_Edge;
static constexpr IVtk_SelectionMode kSelFace = SM_Face;

int GeometryActorSelectOp::toleranceForMode(const SelectMode m)
{
    if (m == SelectMode::GeometryVertex)
        return 40;
    if (m == SelectMode::GeometryEdge)
        return 25;
    if (m == SelectMode::GeometrySolid)
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

GeometryActorSelectOpFactory::GeometryActorSelectOpFactory() = default;
GeometryActorSelectOpFactory::GeometryActorSelectOpFactory(std::weak_ptr<GeometryActor> geometry_actor)
    : geometry_actor_(geometry_actor)
{
}

std::optional<GeometryActorSelectOp> GeometryActorSelectOpFactory::lock()
{
    if (auto geometry_actor = geometry_actor_.lock()) {
        return { geometry_actor };
    }
    return { };
}

GeometryActorSelectOp::GeometryActorSelectOp(std::shared_ptr<GeometryActor> geometry_actor)
    : geometry_actor_(geometry_actor)
{
    if (!geometry_actor_) {
        throw std::runtime_error("GeometryActorSelectOp: geometry_actor is nullptr");
    }
}

IVtk_IdType GeometryActorSelectOp::getShapeId() const
{
    return geometry_actor_->occ_shape_->GetId();
}

void GeometryActorSelectOp::disableSelectionModes(IVtkTools_ShapePicker* picker) const
{
    if (!picker)
        return;

    const OccShapeHandle& occ = geometry_actor_->occ_shape_;

    picker->SetSelectionMode(occ, kSelFace, false);
    picker->SetSelectionMode(occ, kSelEdge, false);
    picker->SetSelectionMode(occ, kSelVertex, false);
}

void GeometryActorSelectOp::enableSelectionMode(IVtkTools_ShapePicker* picker, SelectMode mode) const
{
    if (!picker)
        return;

    const OccShapeHandle& occ = geometry_actor_->occ_shape_;

    switch (mode) {
    case SelectMode::GeometryFace:
    case SelectMode::GeometrySolid:
        picker->SetSelectionMode(occ, kSelFace, true);
        break;
    case SelectMode::GeometryEdge:
        picker->SetSelectionMode(occ, kSelEdge, true);
        break;
    case SelectMode::GeometryVertex:
        picker->SetSelectionMode(occ, kSelVertex, true);
        break;
    }
}

std::optional<Index> GeometryActorSelectOp::resolvePickedSubshape(IVtkTools_ShapePicker* picker,
    IVtk_IdType shapeId, SelectMode mode, IVtk_IdType& out_sub_id) const
{
    if (!picker)
        return std::nullopt;

    TopAbs_ShapeEnum wantType;
    if (mode == SelectMode::GeometryFace)
        wantType = TopAbs_FACE;
    else if (mode == SelectMode::GeometryEdge)
        wantType = TopAbs_EDGE;
    else if (mode == SelectMode::GeometryVertex)
        wantType = TopAbs_VERTEX;
    else
        return std::nullopt;

    out_sub_id = -1;
    if (!firstId(picker->GetPickedSubShapesIds(shapeId, false), out_sub_id))
        return std::nullopt;

    const GeometrySubshapeIndex* geomIndex = geometry_actor_->geometry_index_;
    if (!geomIndex)
        return std::nullopt;

    ElementEnum::Type elemType = ElementEnum::None;
    return mapSubshapeToGeomId(geometry_actor_->occ_shape_, *geomIndex, wantType, out_sub_id, elemType);
}

bool GeometryActorSelectOp::resolvePickedSolid(IVtkTools_ShapePicker* picker, IVtk_IdType shapeId,
    GeomSolidId& out_solid_id, std::vector<IVtk_IdType>& out_face_sub_ids) const
{
    if (!picker)
        return false;

    IVtk_IdType subId = -1;
    if (!firstId(picker->GetPickedSubShapesIds(shapeId, false), subId))
        return false;

    const OccShapeHandle& occ = geometry_actor_->occ_shape_;
    const TopoDS_Shape& pickedSub = occ->GetSubShape(subId);
    if (pickedSub.IsNull())
        return false;

    const GeometrySubshapeIndex* geomIndex = geometry_actor_->geometry_index_;
    if (!geomIndex)
        return false;

    const int solidTi = GeometrySubshapeIndex::typeIndex(TopAbs_SOLID);

    TopoDS_Shape solidShape;
    if (pickedSub.ShapeType() == TopAbs_SOLID) {
        solidShape = pickedSub;
    } else {
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
            return false;
    }

    const int localTypeId = geomIndex->type_maps[solidTi].FindIndex(solidShape);
    if (localTypeId <= 0)
        return false;

    GeomSolidId gid = geomIndex->solidGlobalId(localTypeId);
    if (gid == kInvalidGeomSolidId)
        return false;

    out_solid_id = gid;
    out_face_sub_ids.clear();
    for (TopExp_Explorer expF(solidShape, TopAbs_FACE); expF.More(); expF.Next()) {
        IVtk_IdType faceSubId = occ->GetSubShapeId(expF.Current());
        if (faceSubId >= 0)
            out_face_sub_ids.push_back(faceSubId);
    }
    return true;
}

vtkSmartPointer<IVtkTools_SubPolyDataFilter> GeometryActorSelectOp::buildHighlight(SelectMode mode)
{
    vtkSmartPointer<IVtkTools_SubPolyDataFilter> filter;

    const bool useLine = (mode == SelectMode::GeometryEdge || mode == SelectMode::GeometryVertex);
    vtkPolyData* source = useLine ? geometry_actor_->line_only_.GetPointer()
                                  : geometry_actor_->poly_only_.GetPointer();
    vtkDataArray* idArray = useLine ? geometry_actor_->line_sub_id_array_.GetPointer()
                                    : geometry_actor_->poly_sub_id_array_.GetPointer();
    if (!source)
        return { };

    filter = vtkSmartPointer<IVtkTools_SubPolyDataFilter>::New();
    filter->SetInputData(source);
    filter->SetDoFiltering(true);
    if (idArray && idArray->GetName())
        filter->SetIdsArrayName(idArray->GetName());

    return filter;
}

vtkActor& GeometryActorSelectOp::getPolyActor()
{
    return *geometry_actor_->poly_actor_;
}
