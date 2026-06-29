#include "GeometrySelectorHighlight.h"
#include "GeometryActor.h"
#include "GeometrySubshapeIndex.h"
#include "Core.h"

#include <IVtkOCC_Shape.hxx>
#include <IVtkTools_ShapePicker.hxx>
#include <IVtk_Types.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>

#include <vtkActor.h>
#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkUnsignedCharArray.h>

#include <spdlog/spdlog.h>

#include <unordered_map>

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

static vtkPolyData* actorPoly(vtkActor* a)
{
    if (!a)
        return nullptr;
    auto* m = vtkPolyDataMapper::SafeDownCast(a->GetMapper());
    if (!m)
        return nullptr;
    return m->GetInput();
}

static vtkUnsignedCharArray* cellColors(vtkPolyData* pd)
{
    if (!pd)
        return nullptr;
    return vtkUnsignedCharArray::SafeDownCast(pd->GetCellData()->GetScalars());
}

static void readBaseRgb(vtkUnsignedCharArray* colors, unsigned char outBase[3], const unsigned char fallback[3])
{
    outBase[0] = fallback[0];
    outBase[1] = fallback[1];
    outBase[2] = fallback[2];
    if (!colors || colors->GetNumberOfTuples() <= 0 || colors->GetNumberOfComponents() != 3)
        return;
    unsigned char rgb[3] { fallback[0], fallback[1], fallback[2] };
    colors->GetTypedTuple(0, rgb);
    outBase[0] = rgb[0];
    outBase[1] = rgb[1];
    outBase[2] = rgb[2];
}

static vtkDataArray* detectSubIdArrayByValue(vtkPolyData* pd, IVtk_IdType pickedSubId, const char* tag)
{
    if (!pd)
        return nullptr;
    vtkCellData* cd = pd->GetCellData();
    if (!cd)
        return nullptr;

    vtkDataArray* best = nullptr;
    vtkIdType bestHits = 0;
    const vtkIdType nCells = pd->GetNumberOfCells();

    for (int i = 0; i < cd->GetNumberOfArrays(); ++i) {
        vtkDataArray* a = cd->GetArray(i);
        if (!a)
            continue;
        if (a->GetNumberOfComponents() != 1)
            continue;
        if (a->GetNumberOfTuples() != nCells)
            continue;

        vtkIdType hits = 0;
        for (vtkIdType c = 0; c < nCells; ++c) {
            if (static_cast<IVtk_IdType>(a->GetTuple1(c)) == pickedSubId)
                ++hits;
        }
        if (hits > bestHits) {
            bestHits = hits;
            best = a;
        }
    }

    if (best && bestHits > 0) {
        spdlog::info("[{}] detected subId array='{}' hits={}/{} for subId={}",
            tag, (best->GetName() ? best->GetName() : "(null)"), static_cast<int>(bestHits), static_cast<int>(nCells), pickedSubId);
        return best;
    }

    spdlog::warn("[{}] cannot detect subId array by picked subId={}. Dump arrays:", tag, pickedSubId);
    for (int i = 0; i < cd->GetNumberOfArrays(); ++i) {
        vtkDataArray* a = cd->GetArray(i);
        if (!a)
            continue;
        spdlog::warn("  CellData[{}] name='{}' comps={} tuples={} type={}",
            i, (a->GetName() ? a->GetName() : "(null)"),
            a->GetNumberOfComponents(), static_cast<int>(a->GetNumberOfTuples()), a->GetDataType());
    }
    return nullptr;
}

static void recolorBySelectedSubIds(vtkPolyData* pd,
    vtkDataArray* subIdArr,
    vtkUnsignedCharArray* colors,
    const std::unordered_map<IVtk_IdType, Index>& selected,
    const unsigned char base[3])
{
    if (!pd || !subIdArr || !colors)
        return;

    const vtkIdType n = pd->GetNumberOfCells();
    for (vtkIdType c = 0; c < n; ++c) {
        const IVtk_IdType sid = static_cast<IVtk_IdType>(subIdArr->GetTuple1(c));
        unsigned char rgb[3] { base[0], base[1], base[2] };
        if (selected.count(sid)) {
            rgb[0] = 255;
            rgb[1] = 0;
            rgb[2] = 0;
        }
        colors->SetTypedTuple(c, rgb);
    }

    colors->Modified();
    pd->GetCellData()->Modified();
    pd->Modified();
}

static void resetColorsToBase(vtkPolyData* pd, vtkUnsignedCharArray* colors, const unsigned char base[3])
{
    if (!pd || !colors)
        return;
    const vtkIdType n = pd->GetNumberOfCells();
    for (vtkIdType c = 0; c < n; ++c)
        colors->SetTypedTuple(c, base);

    colors->Modified();
    pd->GetCellData()->Modified();
    pd->Modified();
}

struct GeomTargetData {
    vtkActor* polyActor = nullptr;
    vtkActor* lineActor = nullptr;
    OccShapeHandle occShape;
    const GeometrySubshapeIndex* geomIndex = nullptr;

    bool valid() const { return !occShape.IsNull(); }
};

static GeomTargetData getTargetData(GeometryActorSelectOpFactory& factory)
{
    GeomTargetData d;
    auto op = factory.lock();
    if (!op)
        return d;
    d.polyActor = dynamic_cast<vtkActor*>(&op->getPolyActor());
    d.lineActor = dynamic_cast<vtkActor*>(&op->getLineActor());
    d.occShape = op->getOccShape();
    d.geomIndex = op->getGeometryIndex();
    return d;
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
    face_sub_id_arr_ = nullptr;

    auto op = geom_actor_.lock();
    if (op) {
        auto* polyActor = dynamic_cast<vtkActor*>(&op->getPolyActor());
        vtkPolyData* pd = actorPoly(polyActor);
        vtkUnsignedCharArray* colors = cellColors(pd);
        if (pd && colors)
            resetColorsToBase(pd, colors, face_base_);
    }
}

GeometrySelectionVtk GeometryFaceSelectorHighlight::get() const
{
    GeometrySelectionVtk s;
    s.type = ElementEnum::Face;
    s.component_id = -1;
    for (const auto& [subId, geomId] : selections_) {
        s.ids.push_back(geomId);
    }
    return s;
}

void GeometryFaceSelectorHighlight::ensureTargetInit()
{
    if (target_initialized_)
        return;
    auto op = geom_actor_.lock();
    if (!op)
        return;
    auto* polyActor = dynamic_cast<vtkActor*>(&op->getPolyActor());
    vtkPolyData* pd = actorPoly(polyActor);
    vtkUnsignedCharArray* colors = cellColors(pd);
    const unsigned char fallback[3] = { 200, 200, 200 };
    readBaseRgb(colors, face_base_, fallback);
    if (!pd || !colors) {
        spdlog::warn("Face actor has no polydata or no CellData->Scalars. "
                     "Need GeometryActor::loadShape to create CellColors scalars.");
    }
    target_initialized_ = true;
}

void GeometryFaceSelectorHighlight::select(double posx, double posy)
{
    auto target = getTargetData(geom_actor_);
    if (!target.polyActor || !target.valid() || !target.geomIndex)
        return;

    ensureTargetInit();

    const int n = picker_->Pick(posx, posy, 0.0, renderer_);
    if (n <= 0)
        return;

    IVtk_IdType shapeId = -1;
    if (!firstId(picker_->GetPickedShapesIds(false), shapeId))
        return;

    IVtk_IdType subId = -1;
    if (!firstId(picker_->GetPickedSubShapesIds(shapeId, false), subId))
        return;

    ElementEnum::Type elemType = ElementEnum::None;
    auto geomId = mapSubshapeToGeomId(target.occShape, *target.geomIndex, TopAbs_FACE, subId, elemType);
    if (!geomId)
        return;

    auto [it, inserted] = selections_.insert_or_assign(subId, *geomId);
    if (!inserted)
        selections_.erase(it);

    vtkPolyData* pd = actorPoly(target.polyActor);
    if (!face_sub_id_arr_ && pd)
        face_sub_id_arr_ = detectSubIdArrayByValue(pd, subId, "FacePd");

    applyHighlight();
}

void GeometryFaceSelectorHighlight::applyHighlight()
{
    auto target = getTargetData(geom_actor_);
    vtkPolyData* pd = actorPoly(target.polyActor);
    vtkUnsignedCharArray* colors = cellColors(pd);

    if (!pd || !colors || !face_sub_id_arr_)
        return;

    recolorBySelectedSubIds(pd, face_sub_id_arr_, colors, selections_, face_base_);

    if (renderer_ && renderer_->GetRenderWindow())
        renderer_->GetRenderWindow()->Render();
}

void GeometryFaceSelectorHighlight::setCurGeomActor(GeometryActorSelectOpFactory geom_actor)
{
    auto oldTarget = getTargetData(geom_actor_);
    if (oldTarget.valid()) {
        picker_->SetSelectionMode(oldTarget.occShape, kSelFace, false);
        picker_->SetSelectionMode(oldTarget.occShape, kSelEdge, false);
        picker_->SetSelectionMode(oldTarget.occShape, kSelVertex, false);
        if (oldTarget.polyActor) {
            picker_->SetSelectionMode(oldTarget.polyActor, kSelFace, false);
            picker_->SetSelectionMode(oldTarget.polyActor, kSelEdge, false);
            picker_->SetSelectionMode(oldTarget.polyActor, kSelVertex, false);
        }
        if (oldTarget.lineActor) {
            picker_->SetSelectionMode(oldTarget.lineActor, kSelFace, false);
            picker_->SetSelectionMode(oldTarget.lineActor, kSelEdge, false);
            picker_->SetSelectionMode(oldTarget.lineActor, kSelVertex, false);
        }
        picker_->InitializePickList();
    }
    clear();
    geom_actor_ = std::move(geom_actor);
    target_initialized_ = false;
    configurePicker();
}


void GeometryFaceSelectorHighlight::configurePicker()
{
    auto target = getTargetData(geom_actor_);
    if (!target.polyActor || !target.valid())
        return;

    picker_->PickFromListOn();
    picker_->InitializePickList();
    picker_->AddPickList(target.polyActor);

    picker_->SetPixelTolerance(toleranceForMode(SelectMode::Face));

    picker_->SetSelectionMode(target.occShape, kSelFace, false);
    picker_->SetSelectionMode(target.occShape, kSelEdge, false);
    picker_->SetSelectionMode(target.occShape, kSelVertex, false);
    picker_->SetSelectionMode(target.occShape, kSelFace, true);

    picker_->SetSelectionMode(target.polyActor, kSelFace, true);
    picker_->SetSelectionMode(target.polyActor, kSelEdge, false);
    picker_->SetSelectionMode(target.polyActor, kSelVertex, false);

    if (target.lineActor) {
        picker_->SetSelectionMode(target.lineActor, kSelFace, false);
        picker_->SetSelectionMode(target.lineActor, kSelEdge, false);
        picker_->SetSelectionMode(target.lineActor, kSelVertex, false);
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
    line_sub_id_arr_ = nullptr;

    auto op = geom_actor_.lock();
    if (op) {
        auto* lineActor = dynamic_cast<vtkActor*>(&op->getLineActor());
        vtkPolyData* pd = actorPoly(lineActor);
        vtkUnsignedCharArray* colors = cellColors(pd);
        if (pd && colors)
            resetColorsToBase(pd, colors, line_base_);
    }
}

GeometrySelectionVtk GeometryEdgeSelectorHighlight::get() const
{
    GeometrySelectionVtk s;
    s.type = ElementEnum::Edge;
    s.component_id = -1;
    for (const auto& [subId, geomId] : selections_) {
        s.ids.push_back(geomId);
    }
    return s;
}

void GeometryEdgeSelectorHighlight::ensureTargetInit()
{
    if (target_initialized_)
        return;
    auto op = geom_actor_.lock();
    if (!op)
        return;
    auto* lineActor = dynamic_cast<vtkActor*>(&op->getLineActor());
    vtkPolyData* pd = actorPoly(lineActor);
    vtkUnsignedCharArray* colors = cellColors(pd);
    const unsigned char fallback[3] = { 0, 0, 0 };
    readBaseRgb(colors, line_base_, fallback);
    if (!pd || !colors) {
        spdlog::warn("Line actor has no polydata or no CellData->Scalars. "
                     "Need GeometryActor::loadShape to create CellColors scalars.");
    }
    target_initialized_ = true;
}

void GeometryEdgeSelectorHighlight::select(double posx, double posy)
{
    auto target = getTargetData(geom_actor_);
    if (!target.lineActor || !target.valid() || !target.geomIndex)
        return;

    ensureTargetInit();

    const int n = picker_->Pick(posx, posy, 0.0, renderer_);
    if (n <= 0)
        return;

    IVtk_IdType shapeId = -1;
    if (!firstId(picker_->GetPickedShapesIds(false), shapeId))
        return;

    IVtk_IdType subId = -1;
    if (!firstId(picker_->GetPickedSubShapesIds(shapeId, false), subId))
        return;

    ElementEnum::Type elemType = ElementEnum::None;
    auto geomId = mapSubshapeToGeomId(target.occShape, *target.geomIndex, TopAbs_EDGE, subId, elemType);
    if (!geomId)
        return;

    auto [it, inserted] = selections_.insert_or_assign(subId, *geomId);
    if (!inserted)
        selections_.erase(it);

    vtkPolyData* pd = actorPoly(target.lineActor);
    if (!line_sub_id_arr_ && pd)
        line_sub_id_arr_ = detectSubIdArrayByValue(pd, subId, "LinePd");

    applyHighlight();
}

void GeometryEdgeSelectorHighlight::applyHighlight()
{
    auto target = getTargetData(geom_actor_);
    vtkPolyData* pd = actorPoly(target.lineActor);
    vtkUnsignedCharArray* colors = cellColors(pd);

    if (!pd || !colors || !line_sub_id_arr_)
        return;

    recolorBySelectedSubIds(pd, line_sub_id_arr_, colors, selections_, line_base_);

    if (renderer_ && renderer_->GetRenderWindow())
        renderer_->GetRenderWindow()->Render();
}

void GeometryEdgeSelectorHighlight::setCurGeomActor(GeometryActorSelectOpFactory geom_actor)
{
    auto oldTarget = getTargetData(geom_actor_);
    if (oldTarget.valid()) {
        picker_->SetSelectionMode(oldTarget.occShape, kSelFace, false);
        picker_->SetSelectionMode(oldTarget.occShape, kSelEdge, false);
        picker_->SetSelectionMode(oldTarget.occShape, kSelVertex, false);
        if (oldTarget.polyActor) {
            picker_->SetSelectionMode(oldTarget.polyActor, kSelFace, false);
            picker_->SetSelectionMode(oldTarget.polyActor, kSelEdge, false);
            picker_->SetSelectionMode(oldTarget.polyActor, kSelVertex, false);
        }
        if (oldTarget.lineActor) {
            picker_->SetSelectionMode(oldTarget.lineActor, kSelFace, false);
            picker_->SetSelectionMode(oldTarget.lineActor, kSelEdge, false);
            picker_->SetSelectionMode(oldTarget.lineActor, kSelVertex, false);
        }
        picker_->InitializePickList();
    }
    clear();
    geom_actor_ = std::move(geom_actor);
    target_initialized_ = false;
    configurePicker();
}

void GeometryEdgeSelectorHighlight::configurePicker()
{
    auto target = getTargetData(geom_actor_);
    if (!target.lineActor || !target.valid())
        return;

    picker_->PickFromListOn();
    picker_->InitializePickList();
    picker_->AddPickList(target.lineActor);
    if (target.polyActor)
        picker_->AddPickList(target.polyActor);

    picker_->SetPixelTolerance(toleranceForMode(SelectMode::Edge));

    picker_->SetSelectionMode(target.occShape, kSelFace, false);
    picker_->SetSelectionMode(target.occShape, kSelEdge, false);
    picker_->SetSelectionMode(target.occShape, kSelVertex, false);
    picker_->SetSelectionMode(target.occShape, kSelEdge, true);

    if (target.polyActor) {
        picker_->SetSelectionMode(target.polyActor, kSelFace, false);
        picker_->SetSelectionMode(target.polyActor, kSelEdge, false);
        picker_->SetSelectionMode(target.polyActor, kSelVertex, false);
    }

    picker_->SetSelectionMode(target.lineActor, kSelEdge, true);
    picker_->SetSelectionMode(target.lineActor, kSelVertex, false);
    picker_->SetSelectionMode(target.lineActor, kSelFace, false);
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
    line_sub_id_arr_ = nullptr;

    auto op = geom_actor_.lock();
    if (op) {
        auto* lineActor = dynamic_cast<vtkActor*>(&op->getLineActor());
        vtkPolyData* pd = actorPoly(lineActor);
        vtkUnsignedCharArray* colors = cellColors(pd);
        if (pd && colors)
            resetColorsToBase(pd, colors, line_base_);
    }
}

GeometrySelectionVtk GeometryVertexSelectorHighlight::get() const
{
    GeometrySelectionVtk s;
    s.type = ElementEnum::Vertex;
    s.component_id = -1;
    for (const auto& [subId, geomId] : selections_) {
        s.ids.push_back(geomId);
    }
    return s;
}

void GeometryVertexSelectorHighlight::ensureTargetInit()
{
    if (target_initialized_)
        return;
    auto op = geom_actor_.lock();
    if (!op)
        return;
    auto* lineActor = dynamic_cast<vtkActor*>(&op->getLineActor());
    vtkPolyData* pd = actorPoly(lineActor);
    vtkUnsignedCharArray* colors = cellColors(pd);
    const unsigned char fallback[3] = { 0, 0, 0 };
    readBaseRgb(colors, line_base_, fallback);
    if (!pd || !colors) {
        spdlog::warn("Line actor has no polydata or no CellData->Scalars. "
                     "Need GeometryActor::loadShape to create CellColors scalars.");
    }
    target_initialized_ = true;
}

void GeometryVertexSelectorHighlight::select(double posx, double posy)
{
    auto target = getTargetData(geom_actor_);
    if (!target.lineActor || !target.valid() || !target.geomIndex)
        return;

    ensureTargetInit();

    const int n = picker_->Pick(posx, posy, 0.0, renderer_);
    if (n <= 0)
        return;

    IVtk_IdType shapeId = -1;
    if (!firstId(picker_->GetPickedShapesIds(false), shapeId))
        return;

    IVtk_IdType subId = -1;
    if (!firstId(picker_->GetPickedSubShapesIds(shapeId, false), subId))
        return;

    ElementEnum::Type elemType = ElementEnum::None;
    auto geomId = mapSubshapeToGeomId(target.occShape, *target.geomIndex, TopAbs_VERTEX, subId, elemType);
    if (!geomId)
        return;

    auto [it, inserted] = selections_.insert_or_assign(subId, *geomId);
    if (!inserted)
        selections_.erase(it);

    vtkPolyData* pd = actorPoly(target.lineActor);
    if (!line_sub_id_arr_ && pd)
        line_sub_id_arr_ = detectSubIdArrayByValue(pd, subId, "LinePd");

    applyHighlight();
}

void GeometryVertexSelectorHighlight::applyHighlight()
{
    auto target = getTargetData(geom_actor_);
    vtkPolyData* pd = actorPoly(target.lineActor);
    vtkUnsignedCharArray* colors = cellColors(pd);

    if (!pd || !colors || !line_sub_id_arr_)
        return;

    recolorBySelectedSubIds(pd, line_sub_id_arr_, colors, selections_, line_base_);

    if (renderer_ && renderer_->GetRenderWindow())
        renderer_->GetRenderWindow()->Render();
}

void GeometryVertexSelectorHighlight::setCurGeomActor(GeometryActorSelectOpFactory geom_actor)
{
    auto oldTarget = getTargetData(geom_actor_);
    if (oldTarget.valid()) {
        picker_->SetSelectionMode(oldTarget.occShape, kSelFace, false);
        picker_->SetSelectionMode(oldTarget.occShape, kSelEdge, false);
        picker_->SetSelectionMode(oldTarget.occShape, kSelVertex, false);
        if (oldTarget.polyActor) {
            picker_->SetSelectionMode(oldTarget.polyActor, kSelFace, false);
            picker_->SetSelectionMode(oldTarget.polyActor, kSelEdge, false);
            picker_->SetSelectionMode(oldTarget.polyActor, kSelVertex, false);
        }
        if (oldTarget.lineActor) {
            picker_->SetSelectionMode(oldTarget.lineActor, kSelFace, false);
            picker_->SetSelectionMode(oldTarget.lineActor, kSelEdge, false);
            picker_->SetSelectionMode(oldTarget.lineActor, kSelVertex, false);
        }
        picker_->InitializePickList();
    }
    clear();
    geom_actor_ = std::move(geom_actor);
    target_initialized_ = false;
    configurePicker();
}

void GeometryVertexSelectorHighlight::configurePicker()
{
    auto target = getTargetData(geom_actor_);
    if (!target.lineActor || !target.valid())
        return;

    picker_->PickFromListOn();
    picker_->InitializePickList();
    picker_->AddPickList(target.lineActor);
    if (target.polyActor)
        picker_->AddPickList(target.polyActor);

    picker_->SetPixelTolerance(toleranceForMode(SelectMode::Vertex));

    picker_->SetSelectionMode(target.occShape, kSelFace, false);
    picker_->SetSelectionMode(target.occShape, kSelEdge, false);
    picker_->SetSelectionMode(target.occShape, kSelVertex, false);
    picker_->SetSelectionMode(target.occShape, kSelVertex, true);

    if (target.polyActor) {
        picker_->SetSelectionMode(target.polyActor, kSelFace, false);
        picker_->SetSelectionMode(target.polyActor, kSelEdge, false);
        picker_->SetSelectionMode(target.polyActor, kSelVertex, false);
    }

    picker_->SetSelectionMode(target.lineActor, kSelVertex, true);
    picker_->SetSelectionMode(target.lineActor, kSelEdge, false);
    picker_->SetSelectionMode(target.lineActor, kSelFace, false);
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
    face_sub_id_arr_ = nullptr;

    auto op = geom_actor_.lock();
    if (op) {
        auto* polyActor = dynamic_cast<vtkActor*>(&op->getPolyActor());
        vtkPolyData* pd = actorPoly(polyActor);
        vtkUnsignedCharArray* colors = cellColors(pd);
        if (pd && colors)
            resetColorsToBase(pd, colors, face_base_);
    }
}

GeometrySelectionVtk GeometrySolidSelectorHighlight::get() const
{
    GeometrySelectionVtk s;
    s.type = ElementEnum::Solid;
    s.component_id = -1;
    for (const auto& [subId, geomId] : selections_) {
        s.ids.push_back(geomId);
    }
    return s;
}

void GeometrySolidSelectorHighlight::ensureTargetInit()
{
    if (target_initialized_)
        return;
    auto op = geom_actor_.lock();
    if (!op)
        return;
    auto* polyActor = dynamic_cast<vtkActor*>(&op->getPolyActor());
    vtkPolyData* pd = actorPoly(polyActor);
    vtkUnsignedCharArray* colors = cellColors(pd);
    const unsigned char fallback[3] = { 200, 200, 200 };
    readBaseRgb(colors, face_base_, fallback);
    if (!pd || !colors) {
        spdlog::warn("Solid actor has no polydata or no CellData->Scalars. "
                      "Need GeometryActor::loadShape to create CellColors scalars.");
    }
    target_initialized_ = true;
}

void GeometrySolidSelectorHighlight::select(double posx, double posy)
{
    auto target = getTargetData(geom_actor_);
    if (!target.polyActor || !target.valid() || !target.geomIndex)
        return;

    ensureTargetInit();

    const int n = picker_->Pick(posx, posy, 0.0, renderer_);
    if (n <= 0)
        return;

    IVtk_IdType shapeId = -1;
    if (!firstId(picker_->GetPickedShapesIds(false), shapeId))
        return;

    IVtk_IdType subId = -1;
    if (!firstId(picker_->GetPickedSubShapesIds(shapeId, false), subId))
        return;

    const TopoDS_Shape& pickedSub = target.occShape->GetSubShape(subId);
    if (pickedSub.IsNull())
        return;

    TopoDS_Shape solidShape;
    if (pickedSub.ShapeType() == TopAbs_SOLID) {
        solidShape = pickedSub;
    } else {
        const int solidTi = GeometrySubshapeIndex::typeIndex(TopAbs_SOLID);
        const int nSolids = target.geomIndex->type_maps[solidTi].Extent();
        for (int localId = 1; localId <= nSolids; ++localId) {
            const TopoDS_Shape& solid = target.geomIndex->type_maps[solidTi].FindKey(localId);
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
    const int localTypeId = target.geomIndex->type_maps[solidTi].FindIndex(solidShape);
    if (localTypeId <= 0)
        return;

    GeomSolidId gid = target.geomIndex->solidGlobalId(localTypeId);
    if (gid == kInvalidGeomSolidId)
        return;

    auto [it, inserted] = selections_.insert_or_assign(gid, gid);
    if (!inserted) {
        selections_.erase(it);
    }

    for (TopExp_Explorer expF(solidShape, TopAbs_FACE); expF.More(); expF.Next()) {
        const TopoDS_Shape& face = expF.Current();
        IVtk_IdType faceSubId = target.occShape->GetSubShapeId(face);
        if (faceSubId >= 0) {
            if (!inserted)
                highlighted_face_ids_.erase(faceSubId);
            else
                highlighted_face_ids_.insert(faceSubId);
        }
    }

    vtkPolyData* pd = actorPoly(target.polyActor);
    if (!face_sub_id_arr_ && pd)
        face_sub_id_arr_ = detectSubIdArrayByValue(pd, subId, "SolidPd");

    applyHighlight();
}

void GeometrySolidSelectorHighlight::applyHighlight()
{
    auto target = getTargetData(geom_actor_);
    vtkPolyData* pd = actorPoly(target.polyActor);
    vtkUnsignedCharArray* colors = cellColors(pd);

    if (!pd || !colors || !face_sub_id_arr_)
        return;

    const vtkIdType n = pd->GetNumberOfCells();
    for (vtkIdType c = 0; c < n; ++c) {
        const IVtk_IdType sid = static_cast<IVtk_IdType>(face_sub_id_arr_->GetTuple1(c));
        unsigned char rgb[3] { face_base_[0], face_base_[1], face_base_[2] };
        if (highlighted_face_ids_.count(sid)) {
            rgb[0] = 255;
            rgb[1] = 0;
            rgb[2] = 0;
        }
        colors->SetTypedTuple(c, rgb);
    }

    colors->Modified();
    pd->GetCellData()->Modified();
    pd->Modified();

    if (renderer_ && renderer_->GetRenderWindow())
        renderer_->GetRenderWindow()->Render();
}

void GeometrySolidSelectorHighlight::setCurGeomActor(GeometryActorSelectOpFactory geom_actor)
{
    auto oldTarget = getTargetData(geom_actor_);
    if (oldTarget.valid()) {
        picker_->SetSelectionMode(oldTarget.occShape, kSelFace, false);
        picker_->SetSelectionMode(oldTarget.occShape, kSelEdge, false);
        picker_->SetSelectionMode(oldTarget.occShape, kSelVertex, false);
        if (oldTarget.polyActor) {
            picker_->SetSelectionMode(oldTarget.polyActor, kSelFace, false);
            picker_->SetSelectionMode(oldTarget.polyActor, kSelEdge, false);
            picker_->SetSelectionMode(oldTarget.polyActor, kSelVertex, false);
        }
        if (oldTarget.lineActor) {
            picker_->SetSelectionMode(oldTarget.lineActor, kSelFace, false);
            picker_->SetSelectionMode(oldTarget.lineActor, kSelEdge, false);
            picker_->SetSelectionMode(oldTarget.lineActor, kSelVertex, false);
        }
        picker_->InitializePickList();
    }
    clear();
    geom_actor_ = std::move(geom_actor);
    target_initialized_ = false;
    configurePicker();
}

void GeometrySolidSelectorHighlight::configurePicker()
{
    auto target = getTargetData(geom_actor_);
    if (!target.polyActor || !target.valid()) {
        if (picker_)
            picker_->InitializePickList();
        return;
    }

    picker_->PickFromListOn();
    picker_->InitializePickList();
    picker_->AddPickList(target.polyActor);

    picker_->SetPixelTolerance(toleranceForMode(SelectMode::Solid));

    picker_->SetSelectionMode(target.occShape, kSelFace, false);
    picker_->SetSelectionMode(target.occShape, kSelEdge, false);
    picker_->SetSelectionMode(target.occShape, kSelVertex, false);
    picker_->SetSelectionMode(target.occShape, kSelFace, true);

    picker_->SetSelectionMode(target.polyActor, kSelFace, true);
    picker_->SetSelectionMode(target.polyActor, kSelEdge, false);
    picker_->SetSelectionMode(target.polyActor, kSelVertex, false);

    if (target.lineActor) {
        picker_->SetSelectionMode(target.lineActor, kSelFace, false);
        picker_->SetSelectionMode(target.lineActor, kSelEdge, false);
        picker_->SetSelectionMode(target.lineActor, kSelVertex, false);
    }
}
