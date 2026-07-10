#include "GeometryActorSelectOp.h"
#include "GeometryActor.h"
#include "GeometrySubshapeIndex.h"
#include "Selection.h"
#include "Core.h"

#include <vtkActor.h>
#include <vtkDataArray.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <IVtkOCC_Shape.hxx>
#include <IVtkTools_ShapePicker.hxx>
#include <IVtkTools_SubPolyDataFilter.hxx>
#include <IVtk_Types.hxx>
#include <NCollection_List.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>

static constexpr IVtk_SelectionMode kSelVertex = SM_Vertex;
static constexpr IVtk_SelectionMode kSelEdge = SM_Edge;
static constexpr IVtk_SelectionMode kSelFace = SM_Face;

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
    return {};
}

GeometryActorSelectOp::GeometryActorSelectOp(std::shared_ptr<GeometryActor> geometry_actor)
    : geometry_actor_(geometry_actor)
{
    if (!geometry_actor_) {
        throw std::runtime_error("GeometryActorSelectOp: geometry_actor is nullptr");
    }
}

void GeometryActorSelectOp::disablePickerModes(IVtkTools_ShapePicker* picker)
{
    if (!picker)
        return;

    const OccShapeHandle& occ = geometry_actor_->occ_shape_;
    vtkActor* poly = geometry_actor_->poly_actor_.GetPointer();
    vtkActor* line = geometry_actor_->line_actor_.GetPointer();

    picker->SetSelectionMode(occ, kSelFace, false);
    picker->SetSelectionMode(occ, kSelEdge, false);
    picker->SetSelectionMode(occ, kSelVertex, false);
    picker->SetSelectionMode(poly, kSelFace, false);
    picker->SetSelectionMode(poly, kSelEdge, false);
    picker->SetSelectionMode(poly, kSelVertex, false);
    picker->SetSelectionMode(line, kSelFace, false);
    picker->SetSelectionMode(line, kSelEdge, false);
    picker->SetSelectionMode(line, kSelVertex, false);
    picker->InitializePickList();
}

void GeometryActorSelectOp::configurePicker(IVtkTools_ShapePicker* picker, SelectMode mode)
{
    if (!picker)
        return;

    const OccShapeHandle& occ = geometry_actor_->occ_shape_;
    vtkActor* poly = geometry_actor_->poly_actor_.GetPointer();
    vtkActor* line = geometry_actor_->line_actor_.GetPointer();

    const bool useLine = (mode == SelectMode::Edge || mode == SelectMode::Vertex);

    picker->PickFromListOn();
    picker->InitializePickList();
    picker->AddPickList(useLine ? line : poly);
    if (useLine)
        picker->AddPickList(poly);
    picker->SetPixelTolerance(toleranceForMode(mode));

    picker->SetSelectionMode(occ, kSelFace, false);
    picker->SetSelectionMode(occ, kSelEdge, false);
    picker->SetSelectionMode(occ, kSelVertex, false);
    picker->SetSelectionMode(poly, kSelFace, false);
    picker->SetSelectionMode(poly, kSelEdge, false);
    picker->SetSelectionMode(poly, kSelVertex, false);
    picker->SetSelectionMode(line, kSelFace, false);
    picker->SetSelectionMode(line, kSelEdge, false);
    picker->SetSelectionMode(line, kSelVertex, false);

    switch (mode) {
    case SelectMode::Face:
    case SelectMode::Solid:
        picker->SetSelectionMode(occ, kSelFace, true);
        picker->SetSelectionMode(poly, kSelFace, true);
        break;
    case SelectMode::Edge:
        picker->SetSelectionMode(occ, kSelEdge, true);
        picker->SetSelectionMode(line, kSelEdge, true);
        break;
    case SelectMode::Vertex:
        picker->SetSelectionMode(occ, kSelVertex, true);
        picker->SetSelectionMode(line, kSelVertex, true);
        break;
    default:
        break;
    }
}

std::optional<Index> GeometryActorSelectOp::pickSubshape(IVtkTools_ShapePicker* picker, vtkRenderer* renderer,
    double posx, double posy, SelectMode mode, IVtk_IdType& out_sub_id)
{
    if (!picker)
        return std::nullopt;

    TopAbs_ShapeEnum wantType;
    if (mode == SelectMode::Face)
        wantType = TopAbs_FACE;
    else if (mode == SelectMode::Edge)
        wantType = TopAbs_EDGE;
    else if (mode == SelectMode::Vertex)
        wantType = TopAbs_VERTEX;
    else
        return std::nullopt;

    const int n = picker->Pick(posx, posy, 0.0, renderer);
    if (n <= 0)
        return std::nullopt;

    IVtk_IdType shapeId = -1;
    if (!firstId(picker->GetPickedShapesIds(false), shapeId))
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

bool GeometryActorSelectOp::pickSolid(IVtkTools_ShapePicker* picker, vtkRenderer* renderer, double posx, double posy,
    GeomSolidId& out_solid_id, std::vector<IVtk_IdType>& out_face_sub_ids)
{
    if (!picker)
        return false;

    const int n = picker->Pick(posx, posy, 0.0, renderer);
    if (n <= 0)
        return false;

    IVtk_IdType shapeId = -1;
    if (!firstId(picker->GetPickedShapesIds(false), shapeId))
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

GeometryHighlightPipeline GeometryActorSelectOp::buildHighlight(SelectMode mode)
{
    GeometryHighlightPipeline hl;

    const bool useLine = (mode == SelectMode::Edge || mode == SelectMode::Vertex);
    vtkPolyData* source = useLine ? geometry_actor_->line_only_.GetPointer()
                                  : geometry_actor_->poly_only_.GetPointer();
    vtkDataArray* idArray = useLine ? geometry_actor_->line_sub_id_array_.GetPointer()
                                    : geometry_actor_->poly_sub_id_array_.GetPointer();
    if (!source)
        return hl;

    hl.filter = vtkSmartPointer<IVtkTools_SubPolyDataFilter>::New();
    hl.filter->SetInputData(source);
    hl.filter->SetDoFiltering(true);
    if (idArray && idArray->GetName())
        hl.filter->SetIdsArrayName(idArray->GetName());

    hl.mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    hl.mapper->SetInputConnection(hl.filter->GetOutputPort());

    hl.actor = vtkSmartPointer<vtkActor>::New();
    hl.actor->SetMapper(hl.mapper);
    hl.actor->GetProperty()->SetColor(1.0, 0.0, 0.0);
    hl.actor->SetVisibility(false);
    hl.actor->PickableOff();

    if (useLine) {
        // 相对显示 mapper 默认值：线 (0,-5)，点 (-10)
        hl.mapper->SetRelativeCoincidentTopologyLineOffsetParameters(0, -1);
        hl.mapper->SetRelativeCoincidentTopologyPointOffsetParameter(-2);
        hl.actor->GetProperty()->SetOpacity(0.5);
        hl.actor->GetProperty()->RenderLinesAsTubesOn();
        hl.actor->GetProperty()->SetLineWidth(3.0);
        hl.actor->GetProperty()->SetPointSize(8.0);
        hl.actor->GetProperty()->LightingOff();
    } else {
        // 相对显示 mapper 默认值：多边形 (0,-1)
        hl.mapper->SetRelativeCoincidentTopologyPolygonOffsetParameters(0, -0.5);
        hl.actor->GetProperty()->SetOpacity(1);
    }

    return hl;
}
