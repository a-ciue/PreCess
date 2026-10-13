#include "GeometryActor.h"
#include "GeometryActorManager.h"
#include "GeometryActorSelectOp.h"
#include "GeometryData.h"
#include "GeometryDataVtk.h"
#include "GeometryRegistry.h"
#include "GeometrySelectManager.h"
#include "GeometryTopologyDiagnosticActor.h"
#include "SurfacePrecisionFailure.h"

#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeWire.hxx>
#include <BRepCheck_Analyzer.hxx>
#include <BRepLib.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Geom2d_Line.hxx>
#include <Geom_BSplineSurface.hxx>
#include <IVtkOCC_Shape.hxx>
#include <IVtkTools_ShapeDataSource.hxx>
#include <Poly_Triangulation.hxx>
#include <TColStd_Array1OfInteger.hxx>
#include <TColStd_Array1OfReal.hxx>
#include <TColStd_Array2OfReal.hxx>
#include <TColgp_Array2OfPnt.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Wire.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <gp_Dir2d.hxx>
#include <gp_Pnt2d.hxx>
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkCamera.h>
#include <vtkMapper.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>

#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace {

constexpr Index kComponentId = 123;

struct GeometryScene {
    GeometryData geometry;
    GeometryRegistry registry;
    vtkNew<vtkRenderer> renderer;
    vtkNew<vtkRenderWindow> window;
    GeometryActorManager manager;

    explicit GeometryScene(const TopoDS_Shape& shape)
    {
        geometry.setRootShape(shape);
        geometry.ensureIndexBuilt(registry);
        window->SetOffScreenRendering(1);
        window->SetMultiSamples(0);
        window->SetSize(600, 600);
        window->AddRenderer(renderer);
        manager.bindRender(renderer);
    }

    void load()
    {
        manager.loadGeometry({ *geometry.rootShape, kComponentId, &geometry.index });
    }

    std::shared_ptr<GeometryActor> actor() const
    {
        return manager.getComponentActor(kComponentId);
    }

    vtkPolyData* polygons() const
    {
        GeometryActorSelectOp op(actor());
        auto* mapper = vtkPolyDataMapper::SafeDownCast(op.getPolyActor().GetMapper());
        return mapper ? mapper->GetInput() : nullptr;
    }

    std::array<double, 2> project(const gp_Pnt& point)
    {
        renderer->SetWorldPoint(point.X(), point.Y(), point.Z(), 1.0);
        renderer->WorldToDisplay();
        const double* display = renderer->GetDisplayPoint();
        return { display[0], display[1] };
    }

    void lookDownZ(const gp_Pnt& center)
    {
        renderer->GetActiveCamera()->SetPosition(center.X(), center.Y(), center.Z() + 100.0);
        renderer->GetActiveCamera()->SetFocalPoint(center.X(), center.Y(), center.Z());
        renderer->GetActiveCamera()->SetViewUp(0.0, 1.0, 0.0);
        renderer->GetActiveCamera()->ParallelProjectionOn();
        renderer->ResetCamera();
        window->Render();
    }
};

struct TriangulationSnapshot {
    TopoDS_Face face;
    Handle(Poly_Triangulation) triangulation;
    int triangle_count;
    double deflection;
};

std::vector<TriangulationSnapshot> captureTriangulations(const TopoDS_Shape& shape)
{
    std::vector<TriangulationSnapshot> result;
    for (TopExp_Explorer faces(shape, TopAbs_FACE); faces.More(); faces.Next()) {
        TopoDS_Face face = TopoDS::Face(faces.Current());
        TopLoc_Location location;
        auto triangulation = BRep_Tool::Triangulation(face, location);
        result.push_back({ face, triangulation,
            triangulation.IsNull() ? 0 : triangulation->NbTriangles(),
            triangulation.IsNull() ? 0.0 : triangulation->Deflection() });
    }
    return result;
}

void checkTriangulationsUnchanged(const std::vector<TriangulationSnapshot>& snapshots)
{
    for (const auto& snapshot : snapshots) {
        TopLoc_Location location;
        const auto triangulation = BRep_Tool::Triangulation(snapshot.face, location);
        REQUIRE(triangulation == snapshot.triangulation);
        if (!triangulation.IsNull()) {
            CHECK(triangulation->NbTriangles() == snapshot.triangle_count);
            CHECK(triangulation->Deflection() == snapshot.deflection);
        }
    }
}

vtkIdType highlightCellCount(vtkActor& highlight)
{
    auto* data = vtkPartitionedDataSet::SafeDownCast(highlight.GetMapper()->GetInputDataObject(0, 0));
    REQUIRE(data != nullptr);
    vtkIdType count = 0;
    for (unsigned int i = 0; i < data->GetNumberOfPartitions(); ++i) {
        if (auto* poly = vtkPolyData::SafeDownCast(data->GetPartition(i)))
            count += poly->GetNumberOfCells();
    }
    return count;
}

Index findBoxElement(const GeometrySubshapeIndex& index, TopAbs_ShapeEnum type)
{
    const auto& shapes = index.type_maps[GeometrySubshapeIndex::typeIndex(type)];
    for (int local_id = 1; local_id <= shapes.Extent(); ++local_id) {
        bool all_front = true;
        bool all_bottom = true;
        for (TopExp_Explorer vertices(shapes.FindKey(local_id), TopAbs_VERTEX); vertices.More(); vertices.Next()) {
            const gp_Pnt point = BRep_Tool::Pnt(TopoDS::Vertex(vertices.Current()));
            all_front = all_front && std::abs(point.Z() - 10.0) < 1e-7;
            all_bottom = all_bottom && std::abs(point.Y()) < 1e-7;
        }
        if (type == TopAbs_FACE && all_front)
            return index.faceGlobalId(local_id);
        if (type == TopAbs_EDGE && all_front && all_bottom)
            return index.edgeGlobalId(local_id);
        if (type == TopAbs_VERTEX) {
            const gp_Pnt point = BRep_Tool::Pnt(TopoDS::Vertex(shapes.FindKey(local_id)));
            if (point.Distance(gp_Pnt(0.0, 0.0, 10.0)) < 1e-7)
                return index.vertexGlobalId(local_id);
        }
    }
    return -1;
}

TopoDS_Wire makeUvRectangle(const Handle(Geom_Surface) & surface, double low, double high)
{
    const std::array<gp_Pnt2d, 4> points {
        gp_Pnt2d(low, low), gp_Pnt2d(high, low), gp_Pnt2d(high, high), gp_Pnt2d(low, high)
    };
    BRepBuilderAPI_MakeWire wire;
    for (size_t i = 0; i < points.size(); ++i) {
        const gp_Pnt2d& start = points[i];
        const gp_Pnt2d& end = points[(i + 1) % points.size()];
        const Handle(Geom2d_Line) line = new Geom2d_Line(start, gp_Dir2d(end.X() - start.X(), end.Y() - start.Y()));
        BRepBuilderAPI_MakeEdge edge(line, surface, 0.0, start.Distance(end));
        REQUIRE(edge.IsDone());
        wire.Add(edge.Edge());
    }
    REQUIRE(wire.IsDone());
    return wire.Wire();
}

TopoDS_Face makeHoledNurbsFace()
{
    TColgp_Array2OfPnt poles(1, 4, 1, 4);
    TColStd_Array2OfReal weights(1, 4, 1, 4);
    for (int u = 1; u <= 4; ++u) {
        for (int v = 1; v <= 4; ++v) {
            const bool interior = (u == 2 || u == 3) && (v == 2 || v == 3);
            poles.SetValue(u, v, gp_Pnt((u - 1) * 10.0, (v - 1) * 10.0, interior ? 8.0 : 0.0));
            weights.SetValue(u, v, interior ? 1.3 : 1.0);
        }
    }
    TColStd_Array1OfReal knots(1, 2);
    knots.SetValue(1, 0.0);
    knots.SetValue(2, 1.0);
    TColStd_Array1OfInteger multiplicities(1, 2);
    multiplicities.Init(4);
    const Handle(Geom_BSplineSurface) surface = new Geom_BSplineSurface(
        poles, weights, knots, knots, multiplicities, multiplicities, 3, 3);
    TopoDS_Wire inner = makeUvRectangle(surface, 0.35, 0.65);
    inner.Reverse();
    BRepBuilderAPI_MakeFace face(surface, makeUvRectangle(surface, 0.0, 1.0), true);
    face.Add(inner);
    REQUIRE(face.IsDone());
    BRepLib::BuildCurves3d(face.Shape());
    REQUIRE(BRepCheck_Analyzer(face.Shape()).IsValid());
    return face.Face();
}

} // namespace

TEST_CASE("CAD surface standard precision retains the original IVtk default", "[geometry][precision]")
{
    // 独立基准形状避免旧显示链路的三角缓存影响被测业务形状。
    GeometryData reference;
    reference.setRootShape(BRepPrimAPI_MakeSphere(10.0).Shape());
    const OccShapeHandle reference_occ = new IVtkOCC_Shape(*reference.rootShape);
    vtkNew<IVtkTools_ShapeDataSource> reference_source;
    reference_source->SetShape(reference_occ);
    reference_source->Update();

    GeometryScene scene(BRepPrimAPI_MakeSphere(10.0).Shape());
    REQUIRE(scene.manager.getSurfacePrecision() == 1);
    scene.load();
    REQUIRE(scene.polygons() != nullptr);
    GeometryActorSelectOp op(scene.actor());
    CHECK(scene.polygons()->GetNumberOfPolys() == reference_source->GetOutput()->GetNumberOfPolys());
    CHECK(op.getLineOnlyPolyData()->GetNumberOfLines() == reference_source->GetOutput()->GetNumberOfLines());
    CHECK(op.getLineOnlyPolyData()->GetNumberOfVerts() == reference_source->GetOutput()->GetNumberOfVerts());
}

TEST_CASE("CAD surface precision rebuilds in both directions and applies to later imports", "[geometry][precision]")
{
    GeometryScene scene(BRepPrimAPI_MakeSphere(10.0).Shape());
    const auto original_caches = captureTriangulations(*scene.geometry.rootShape);
    for (const auto& cache : original_caches)
        REQUIRE(cache.triangulation.IsNull());
    scene.load();
    checkTriangulationsUnchanged(original_caches);
    const OccShapeHandle original_display = GeometryActorSelectOp(scene.actor()).getOccShape();
    REQUIRE_FALSE(original_display->GetShape().IsSame(*scene.geometry.rootShape));
    std::array<vtkIdType, 4> counts {};
    for (int precision = 0; precision < 4; ++precision) {
        scene.manager.setSurfacePrecision(precision);
        counts[precision] = scene.polygons()->GetNumberOfPolys();
        const auto& display = GeometryActorSelectOp(scene.actor()).getOccShape();
        CHECK(display->GetId() == original_display->GetId());
        CHECK_FALSE(display->GetShape().IsSame(*scene.geometry.rootShape));
        checkTriangulationsUnchanged(original_caches);
    }
    INFO("sphere triangle counts: " << counts[0] << ", " << counts[1] << ", " << counts[2] << ", " << counts[3]);
    REQUIRE(counts[0] > 0);
    REQUIRE(counts[0] < counts[1]);
    REQUIRE(counts[1] < counts[2]);
    REQUIRE(counts[2] < counts[3]);
    scene.manager.setSurfacePrecision(0);
    CHECK(scene.polygons()->GetNumberOfPolys() == counts[0]);
    scene.manager.setSurfacePrecision(3);
    CHECK(scene.polygons()->GetNumberOfPolys() == counts[3]);

    GeometryData later;
    later.setRootShape(BRepPrimAPI_MakeSphere(10.0).Shape());
    later.ensureIndexBuilt(scene.registry);
    scene.manager.loadGeometry({ *later.rootShape, kComponentId + 1, &later.index });
    GeometryActorSelectOp later_op(scene.manager.getComponentActor(kComponentId + 1));
    auto* later_mapper = vtkPolyDataMapper::SafeDownCast(later_op.getPolyActor().GetMapper());
    REQUIRE(later_mapper != nullptr);
    CHECK(later_mapper->GetInput()->GetNumberOfPolys() == counts[3]);

    scene.manager.setSurfacePrecision(-1);
    scene.manager.setSurfacePrecision(4);
    CHECK(scene.manager.getSurfacePrecision() == 3);
    CHECK(scene.polygons()->GetNumberOfPolys() == counts[3]);
}

TEST_CASE("CAD display remeshing preserves shared geometry snapshot caches and render state", "[geometry][precision]")
{
    GeometryScene scene(BRepPrimAPI_MakeSphere(10.0).Shape());
    BRepMesh_IncrementalMesh source_mesh(*scene.geometry.rootShape, 0.7, false, 0.4, true);
    REQUIRE(source_mesh.IsDone());
    const auto original_caches = captureTriangulations(*scene.geometry.rootShape);
    REQUIRE_FALSE(original_caches.empty());
    REQUIRE_FALSE(original_caches.front().triangulation.IsNull());
    const auto snapshot = scene.geometry.clone();
    const auto snapshot_caches = captureTriangulations(*snapshot->rootShape);
    scene.load();
    scene.manager.setCurrentRenderStyle(GeometryRenderStyle::Transparent25);
    scene.manager.setVisibility(kComponentId, false);
    for (int precision : { 3, 0, 2, 1 }) {
        scene.manager.setSurfacePrecision(precision);
        checkTriangulationsUnchanged(original_caches);
        checkTriangulationsUnchanged(snapshot_caches);
        CHECK_FALSE(scene.actor()->isVisible());
        CHECK(scene.actor()->getRenderStyle() == GeometryRenderStyle::Transparent25);
        CHECK(GeometryActorSelectOp(scene.actor()).getPolyActor().GetVisibility() == 0);
    }
    scene.manager.setVisibility(kComponentId, true);
    CHECK(scene.actor()->isVisible());
    CHECK(GeometryActorSelectOp(scene.actor()).getPolyActor().GetProperty()->GetOpacity() == Catch::Approx(0.25));
    scene.manager.setCurrentRenderStyle(GeometryRenderStyle::Hidden);
    scene.manager.setSurfacePrecision(3);
    CHECK_FALSE(scene.actor()->isVisible());
    CHECK(GeometryActorSelectOp(scene.actor()).getPolyActor().GetVisibility() == 0);
}

TEST_CASE("CAD precision refresh preserves original face edge vertex and solid selections", "[geometry][precision][render]")
{
    GeometryScene scene(BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape());
    scene.load();
    vtkNew<vtkActor> highlight;
    highlight->PickableOff();
    scene.renderer->AddActor(highlight);
    GeometrySelectManager selection(*scene.renderer, *highlight, scene.manager.op());
    scene.lookDownZ(gp_Pnt(5.0, 5.0, 5.0));

    struct PickCase {
        SelectMode mode;
        gp_Pnt point;
        Index expected_gid;
    };
    const std::array<PickCase, 4> cases { { { SelectMode::GeometryFace, gp_Pnt(5.0, 5.0, 10.0), findBoxElement(scene.geometry.index, TopAbs_FACE) },
        { SelectMode::GeometryEdge, gp_Pnt(5.0, 0.0, 10.0), findBoxElement(scene.geometry.index, TopAbs_EDGE) },
        { SelectMode::GeometryVertex, gp_Pnt(0.0, 0.0, 10.0), findBoxElement(scene.geometry.index, TopAbs_VERTEX) },
        { SelectMode::GeometrySolid, gp_Pnt(5.0, 5.0, 10.0), scene.geometry.index.solidGlobalId(1) } } };
    for (const auto& pick : cases) {
        INFO("selection mode: " << static_cast<int>(pick.mode));
        REQUIRE(pick.expected_gid >= 0);
        selection.setSelectMode(pick.mode);
        const auto display = scene.project(pick.point);
        selection.select(display[0], display[1]);
        REQUIRE(selection.getSelection()->ids == std::vector<Index> { pick.expected_gid });
        REQUIRE(highlightCellCount(*highlight) > 0);
        for (int precision : { 3, 0, 1 }) {
            scene.manager.setSurfacePrecision(precision);
            selection.refreshSelectionHighlight();
            scene.window->Render();
            CHECK(selection.getSelection()->ids == std::vector<Index> { pick.expected_gid });
            CHECK(highlightCellCount(*highlight) > 0);
        }
        // 同一原始 gid 在重建后仍可再次拾取并取消，覆盖 picker 缓存与映射。
        selection.select(display[0], display[1]);
        CHECK(selection.getSelection()->ids.empty());
        CHECK(highlightCellCount(*highlight) == 0);
    }
}

TEST_CASE("CAD selected curved face highlight follows the rebuilt triangulation", "[geometry][precision][render]")
{
    GeometryScene scene(BRepPrimAPI_MakeSphere(10.0).Shape());
    scene.load();
    scene.manager.setSurfacePrecision(0);
    vtkNew<vtkActor> highlight;
    highlight->PickableOff();
    scene.renderer->AddActor(highlight);
    GeometrySelectManager selection(*scene.renderer, *highlight, scene.manager.op());
    selection.setSelectMode(SelectMode::GeometryFace);
    scene.lookDownZ(gp_Pnt(0.0, 0.0, 0.0));
    // 避开球面极点，使用前半球上普通位置。
    const auto display = scene.project(gp_Pnt(3.0, 2.0, std::sqrt(87.0)));
    selection.select(display[0], display[1]);
    const auto selected_ids = selection.getSelection()->ids;
    REQUIRE(selected_ids == std::vector<Index> { scene.geometry.index.faceGlobalId(1) });
    REQUIRE(highlightCellCount(*highlight) == scene.polygons()->GetNumberOfPolys());
    const vtkIdType coarse_count = highlightCellCount(*highlight);
    scene.manager.setSurfacePrecision(3);
    selection.refreshSelectionHighlight();
    CHECK(selection.getSelection()->ids == selected_ids);
    CHECK(highlightCellCount(*highlight) == scene.polygons()->GetNumberOfPolys());
    CHECK(highlightCellCount(*highlight) > coarse_count);
    scene.manager.setSurfacePrecision(0);
    selection.refreshSelectionHighlight();
    CHECK(selection.getSelection()->ids == selected_ids);
    CHECK(highlightCellCount(*highlight) == coarse_count);

    scene.manager.setVisibility(kComponentId, false);
    scene.manager.setSurfacePrecision(3);
    selection.refreshSelectionHighlight();
    CHECK(selection.getSelection()->ids == selected_ids);
    CHECK(highlightCellCount(*highlight) == 0);
    scene.manager.setVisibility(kComponentId, true);
    selection.refreshSelectionHighlight();
    CHECK(highlightCellCount(*highlight) == scene.polygons()->GetNumberOfPolys());

    selection.setHighlightVisible(false);
    scene.manager.setSurfacePrecision(0);
    selection.refreshSelectionHighlight();
    CHECK(highlight->GetMapper()->GetInputDataObject(0, 0) == nullptr);
    CHECK(selection.getSelection()->ids == selected_ids);
    selection.setHighlightVisible(true);
    CHECK(highlightCellCount(*highlight) == coarse_count);
}

TEST_CASE("CAD precision rebuild retains a trimmed rational B-spline surface hole", "[geometry][precision]")
{
    GeometryScene scene(makeHoledNurbsFace());
    scene.load();
    std::array<vtkIdType, 2> counts {};
    int count_index = 0;
    for (int precision : { 0, 3 }) {
        scene.manager.setSurfacePrecision(precision);
        counts[count_index++] = scene.polygons()->GetNumberOfPolys();
        const OccShapeHandle& display = GeometryActorSelectOp(scene.actor()).getOccShape();
        TopExp_Explorer face(display->GetShape(), TopAbs_FACE);
        REQUIRE(face.More());
        TopLoc_Location location;
        const auto triangulation = BRep_Tool::Triangulation(TopoDS::Face(face.Current()), location);
        REQUIRE_FALSE(triangulation.IsNull());
        REQUIRE(triangulation->HasUVNodes());
        for (int i = 1; i <= triangulation->NbTriangles(); ++i) {
            int a, b, c;
            triangulation->Triangle(i).Get(a, b, c);
            const gp_Pnt2d uv_a = triangulation->UVNode(a);
            const gp_Pnt2d uv_b = triangulation->UVNode(b);
            const gp_Pnt2d uv_c = triangulation->UVNode(c);
            const double u = (uv_a.X() + uv_b.X() + uv_c.X()) / 3.0;
            const double v = (uv_a.Y() + uv_b.Y() + uv_c.Y()) / 3.0;
            REQUIRE_FALSE((u > 0.350001 && u < 0.649999 && v > 0.350001 && v < 0.649999));
        }
    }
    REQUIRE(counts[0] > 0);
    CHECK(counts[1] > counts[0]);
}

TEST_CASE("CAD hidden geometry restores picking and selected highlight after precision rebuild", "[geometry][precision][render]")
{
    GeometryScene scene(BRepPrimAPI_MakeSphere(10.0).Shape());
    scene.load();
    vtkNew<vtkActor> highlight;
    highlight->PickableOff();
    scene.renderer->AddActor(highlight);
    GeometrySelectManager selection(*scene.renderer, *highlight, scene.manager.op());
    selection.setSelectMode(SelectMode::GeometryFace);
    scene.lookDownZ(gp_Pnt(0.0, 0.0, 0.0));
    const auto display = scene.project(gp_Pnt(3.0, 2.0, std::sqrt(87.0)));
    selection.select(display[0], display[1]);
    const auto selected_ids = selection.getSelection()->ids;
    REQUIRE(selected_ids == std::vector<Index> { scene.geometry.index.faceGlobalId(1) });

    // 全局 Hidden 走显示样式入口，区别于对象树的组件可见性开关。
    const std::array<GeometryRenderStyle, 2> restored_styles {
        GeometryRenderStyle::Surface, GeometryRenderStyle::SurfaceWithEdges
    };
    int precision = 3;
    for (const auto style : restored_styles) {
        scene.manager.setCurrentRenderStyle(GeometryRenderStyle::Hidden);
        scene.manager.setSurfacePrecision(precision);
        selection.refreshSelectionHighlight();
        CHECK(selection.getSelection()->ids == selected_ids);
        CHECK(highlightCellCount(*highlight) == 0);

        scene.manager.setCurrentRenderStyle(style);
        selection.refreshSelectionHighlight();
        scene.window->Render();
        REQUIRE(scene.actor()->isVisible());
        CHECK(selection.getSelection()->ids == selected_ids);
        CHECK(highlightCellCount(*highlight) == scene.polygons()->GetNumberOfPolys());

        // 恢复后真正重拾一次：若 Picker 仍处于禁用态，Toggle 会走空拾取清空；
        // 紧接着 Append 必须重新命中，避免把清空误判为恢复成功。
        selection.select(display[0], display[1]);
        REQUIRE(selection.getSelection()->ids.empty());
        selection.select(display[0], display[1], SelectOp::Append);
        CHECK(selection.getSelection()->ids == selected_ids);
        CHECK(highlightCellCount(*highlight) == scene.polygons()->GetNumberOfPolys());
        precision = 0;
    }
}

TEST_CASE("CAD precision changes safely handle no components and an empty compound", "[geometry][precision]")
{
    TopoDS_Compound empty;
    BRep_Builder builder;
    builder.MakeCompound(empty);
    GeometryScene scene(empty);

    REQUIRE_NOTHROW(scene.manager.setSurfacePrecision(0));
    REQUIRE_NOTHROW(scene.manager.setSurfacePrecision(3));
    CHECK(scene.manager.getSurfacePrecision() == 3);
    CHECK_FALSE(scene.manager.hasComponent(kComponentId));
    REQUIRE_NOTHROW(scene.load());
    REQUIRE(scene.actor() != nullptr);

    for (int precision : { 0, 3, 2, 1 }) {
        REQUIRE_NOTHROW(scene.manager.setSurfacePrecision(precision));
        REQUIRE(scene.polygons() != nullptr);
        CHECK(scene.polygons()->GetNumberOfCells() == 0);
        GeometryActorSelectOp op(scene.actor());
        REQUIRE(op.getLineOnlyPolyData() != nullptr);
        CHECK(op.getLineOnlyPolyData()->GetNumberOfCells() == 0);
    }
}

TEST_CASE("CAD actor precision failure preserves display caches and permits retry", "[geometry][precision][failure]")
{
    GeometryScene scene(BRepPrimAPI_MakeSphere(10.0).Shape());
    scene.load();
    const auto actor = scene.actor();
    const auto original_shape = GeometryActorSelectOp(actor).getOccShape();
    const auto caches = captureTriangulations(original_shape->GetShape());
    vtkSmartPointer<vtkPolyData> polygons = scene.polygons();
    const int actor_count = scene.renderer->GetActors()->GetNumberOfItems();
    SurfacePrecisionFailure failure;
    REQUIRE_THROWS_AS(actor->setSurfacePrecision(3), std::runtime_error);
    CHECK(failure.rebuilds() == 1);
    CHECK(actor->getSurfacePrecision() == 1);
    CHECK(scene.polygons() == polygons);
    CHECK(GeometryActorSelectOp(actor).getOccShape() == original_shape);
    CHECK(scene.renderer->GetActors()->GetNumberOfItems() == actor_count);
    checkTriangulationsUnchanged(caches);
    REQUIRE_NOTHROW(actor->setSurfacePrecision(3));
    CHECK(actor->getSurfacePrecision() == 3);
    CHECK(scene.polygons()->GetNumberOfPolys() > polygons->GetNumberOfPolys());
}

TEST_CASE("CAD batch precision failure keeps every component pickable and retries atomically", "[geometry][precision][failure][render]")
{
    GeometryScene scene(BRepPrimAPI_MakeSphere(10.0).Shape());
    scene.load();
    GeometryData later;
    later.setRootShape(BRepPrimAPI_MakeSphere(gp_Pnt(30.0, 0.0, 0.0), 10.0).Shape());
    later.ensureIndexBuilt(scene.registry);
    scene.manager.loadGeometry({ *later.rootShape, kComponentId + 1, &later.index });
    scene.manager.setCurrentRenderStyle(GeometryRenderStyle::Transparent25);
    // 诊断也参与候选准备，失败后保留旧叠加，成功后保留类别及阈值。
    scene.manager.setTopologyDiagnosticSmallFaceArea(2000.0);
    scene.manager.setTopologyDiagnosticCategoryEnabled(
        static_cast<int>(GeometryTopologyDiagnosticCategory::SmallFace), true);
    vtkNew<vtkActor> highlight;
    highlight->PickableOff();
    scene.renderer->AddActor(highlight);
    GeometrySelectManager selection(*scene.renderer, *highlight, scene.manager.op());
    selection.setSelectMode(SelectMode::GeometryFace);
    scene.lookDownZ(gp_Pnt(0.0, 0.0, 0.0));
    const auto display = scene.project(gp_Pnt(3.0, 2.0, std::sqrt(87.0)));
    selection.select(display[0], display[1]);
    const auto selected = selection.getSelection()->ids;
    REQUIRE(selected == std::vector<Index> { scene.geometry.index.faceGlobalId(1) });

    std::array<OccShapeHandle, 2> shapes;
    std::array<vtkSmartPointer<vtkPolyData>, 2> polygons;
    for (int i = 0; i < 2; ++i) {
        GeometryActorSelectOp op(scene.manager.getComponentActor(kComponentId + i));
        shapes[i] = op.getOccShape();
        polygons[i] = vtkPolyDataMapper::SafeDownCast(op.getPolyActor().GetMapper())->GetInput();
        REQUIRE_FALSE(shapes[i]->GetSelectableObject().IsNull());
    }
    const auto first_selectable = shapes[0]->GetSelectableObject();
    const auto second_selectable = shapes[1]->GetSelectableObject();
    const auto first_caches = captureTriangulations(shapes[0]->GetShape());
    const auto second_caches = captureTriangulations(shapes[1]->GetShape());
    const int actor_count = scene.renderer->GetActors()->GetNumberOfItems();
    const auto highlight_count = highlightCellCount(*highlight);
    SurfacePrecisionFailure failure(2);
    REQUIRE_THROWS_AS(scene.manager.setSurfacePrecision(3), std::runtime_error);
    REQUIRE(failure.rebuilds() == 2); // 第一个组件已准备成功，第二个在重建末尾失败。
    CHECK(scene.manager.getSurfacePrecision() == 1);
    CHECK(scene.renderer->GetActors()->GetNumberOfItems() == actor_count);
    CHECK(shapes[0]->GetSelectableObject() == first_selectable);
    CHECK(shapes[1]->GetSelectableObject() == second_selectable);
    checkTriangulationsUnchanged(first_caches);
    checkTriangulationsUnchanged(second_caches);
    for (int i = 0; i < 2; ++i) {
        const auto actor = scene.manager.getComponentActor(kComponentId + i);
        GeometryActorSelectOp op(actor);
        CHECK(actor->getSurfacePrecision() == 1);
        CHECK(op.getOccShape() == shapes[i]);
        CHECK(vtkPolyDataMapper::SafeDownCast(op.getPolyActor().GetMapper())->GetInput() == polygons[i]);
        CHECK(scene.manager.op().getComponentId(&op.getPolyActor()) == kComponentId + i);
    }
    selection.refreshSelectionHighlight();
    CHECK(selection.getSelection()->ids == selected);
    CHECK(highlightCellCount(*highlight) == highlight_count);
    selection.select(display[0], display[1]);
    REQUIRE(selection.getSelection()->ids.empty());
    selection.select(display[0], display[1], SelectOp::Append);
    REQUIRE(selection.getSelection()->ids == selected);

    REQUIRE_NOTHROW(scene.manager.setSurfacePrecision(3));
    CHECK(scene.manager.getSurfacePrecision() == 3);
    CHECK(scene.renderer->GetActors()->GetNumberOfItems() == actor_count);
    for (int i = 0; i < 2; ++i) {
        const auto actor = scene.manager.getComponentActor(kComponentId + i);
        GeometryActorSelectOp op(actor);
        CHECK(actor->getSurfacePrecision() == 3);
        CHECK(actor->getRenderStyle() == GeometryRenderStyle::Transparent25);
        CHECK(op.getPolyActor().GetProperty()->GetOpacity() == Catch::Approx(0.25));
        CHECK(vtkPolyDataMapper::SafeDownCast(op.getPolyActor().GetMapper())->GetInput()->GetNumberOfPolys()
            > polygons[i]->GetNumberOfPolys());
        REQUIRE_FALSE(op.getOccShape()->GetSelectableObject().IsNull());
    }
    selection.refreshSelectionHighlight();
    CHECK(selection.getSelection()->ids == selected);
    CHECK(highlightCellCount(*highlight) > highlight_count);
    scene.window->Render();
    selection.select(display[0], display[1]);
    REQUIRE(selection.getSelection()->ids.empty());
    selection.select(display[0], display[1], SelectOp::Append);
    CHECK(selection.getSelection()->ids == selected);
    scene.manager.loadGeometry({ *later.rootShape, kComponentId + 2, &later.index });
    CHECK(scene.manager.getComponentActor(kComponentId + 2)->getSurfacePrecision() == 3);
}
