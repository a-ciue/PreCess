#include "GeometryActor.h"
#include "GeometryActorManager.h"
#include "GeometryActorSelectOp.h"
#include "GeometryData.h"
#include "GeometryDataVtk.h"
#include "GeometryRegistry.h"
#include "GeometrySelectManager.h"
#include "SurfacePrecisionFailure.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Tool.hxx>
#include <IVtkOCC_Shape.hxx>
#include <IVtkTools_ShapeDataSource.hxx>
#include <IVtkTools_SubPolyDataFilter.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPropCollection.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>

#include <array>
#include <cmath>
#include <memory>
#include <unordered_set>
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

    std::shared_ptr<GeometryActor> actor(Index id = kComponentId) const
    {
        return manager.getComponentActor(id);
    }

    vtkPolyData* polygons(Index id = kComponentId) const
    {
        GeometryActorSelectOp op(actor(id));
        auto* mapper = vtkPolyDataMapper::SafeDownCast(op.getPolyActor().GetMapper());
        return mapper ? mapper->GetInput() : nullptr;
    }

    vtkPolyData* lines(Index id = kComponentId) const
    {
        auto filter = GeometryActorSelectOp(actor(id)).buildHighlight(SelectMode::GeometryEdge);
        return vtkPolyData::SafeDownCast(filter->GetInput());
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

void checkRepick(GeometrySelectManager& selection, const std::array<double, 2>& display, const std::vector<Index>& expected)
{
    CHECK(selection.getSelection()->ids == expected);
    selection.select(display[0], display[1]);
    REQUIRE(selection.getSelection()->ids.empty());
    selection.select(display[0], display[1], SelectOp::Append);
    CHECK(selection.getSelection()->ids == expected);
}

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

} // namespace

TEST_CASE("CAD surface precision rebuilds in both directions and applies to later imports", "[geometry][precision]")
{
    // 独立基准形状避免旧显示链路的三角缓存影响被测业务形状。
    const OccShapeHandle reference_occ = new IVtkOCC_Shape(BRepPrimAPI_MakeSphere(10.0).Shape());
    vtkNew<IVtkTools_ShapeDataSource> reference_source;
    reference_source->SetShape(reference_occ);
    reference_source->Update();

    GeometryScene scene(BRepPrimAPI_MakeSphere(10.0).Shape());
    BRepMesh_IncrementalMesh source_mesh(*scene.geometry.rootShape, 0.7, false, 0.4, true);
    REQUIRE(source_mesh.IsDone());
    const auto original_caches = captureTriangulations(*scene.geometry.rootShape);
    REQUIRE_FALSE(original_caches.empty());
    REQUIRE_FALSE(original_caches.front().triangulation.IsNull());
    const auto snapshot = scene.geometry.clone();
    const auto snapshot_caches = captureTriangulations(*snapshot->rootShape);
    scene.load();
    CHECK(scene.manager.getSurfacePrecision() == 1);
    CHECK(scene.polygons()->GetNumberOfPolys() == reference_source->GetOutput()->GetNumberOfPolys());
    CHECK(scene.lines()->GetNumberOfLines() == reference_source->GetOutput()->GetNumberOfLines());
    CHECK(scene.lines()->GetNumberOfVerts() == reference_source->GetOutput()->GetNumberOfVerts());
    checkTriangulationsUnchanged(original_caches);
    auto* original_polygons = scene.polygons();
    auto* original_actor = &GeometryActorSelectOp(scene.actor()).getPolyActor();
    std::array<vtkIdType, 4> counts {};
    for (int precision = 0; precision < 4; ++precision) {
        scene.manager.setSurfacePrecision(precision);
        counts[precision] = scene.polygons()->GetNumberOfPolys();
        CHECK(scene.actor()->getSurfacePrecision() == precision);
        CHECK(scene.polygons() == original_polygons);
        CHECK(&GeometryActorSelectOp(scene.actor()).getPolyActor() == original_actor);
        checkTriangulationsUnchanged(original_caches);
        checkTriangulationsUnchanged(snapshot_caches);
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
    REQUIRE(scene.polygons(kComponentId + 1) != nullptr);
    CHECK(scene.polygons(kComponentId + 1)->GetNumberOfPolys() == counts[3]);

    // 已有组件替换几何时也沿用窗口目标档位。
    scene.manager.loadGeometry({ *later.rootShape, kComponentId, &later.index });
    CHECK(scene.actor()->getSurfacePrecision() == 3);
    CHECK(scene.polygons()->GetNumberOfPolys() == counts[3]);

    scene.manager.setSurfacePrecision(-1);
    scene.manager.setSurfacePrecision(4);
    CHECK(scene.manager.getSurfacePrecision() == 3);
    CHECK(scene.polygons()->GetNumberOfPolys() == counts[3]);
}

TEST_CASE("CAD nonstandard initial precision supports registered picking before and after loading", "[geometry][precision][render]")
{
    const int initial_precision = GENERATE(0, 3);
    const bool enable_before_load = GENERATE(false, true);
    const auto mode = GENERATE(SelectMode::GeometryVertex, SelectMode::GeometryEdge,
        SelectMode::GeometryFace, SelectMode::GeometrySolid);
    BRepPrimAPI_MakeBox box(10.0, 10.0, 10.0);
    GeometryScene scene(box.Shape());
    BRepMesh_IncrementalMesh source_mesh(*scene.geometry.rootShape, 0.7, false, 0.4, true);
    REQUIRE(source_mesh.IsDone());
    const auto original_caches = captureTriangulations(*scene.geometry.rootShape);
    const auto snapshot = scene.geometry.clone();
    const auto snapshot_caches = captureTriangulations(*snapshot->rootShape);
    scene.manager.setSurfacePrecision(initial_precision);

    vtkNew<vtkActor> highlight;
    highlight->PickableOff();
    scene.renderer->AddActor(highlight);
    GeometrySelectManager selection(*scene.renderer, *highlight, scene.manager.op());
    if (enable_before_load)
        selection.setSelectMode(mode);
    scene.load();
    if (!enable_before_load)
        selection.setSelectMode(mode);
    scene.renderer->GetActiveCamera()->SetPosition(35.0, 45.0, 55.0);
    scene.renderer->GetActiveCamera()->SetFocalPoint(5.0, 5.0, 5.0);
    scene.renderer->GetActiveCamera()->SetViewUp(0.0, 0.0, 1.0);
    scene.renderer->GetActiveCamera()->ParallelProjectionOn();
    scene.renderer->ResetCamera();
    scene.window->Render();

    // 使用已知几何位置独立确定预期业务 ID，避免只检查“拾取非空”。
    gp_Pnt point(5.0, 5.0, 10.0);
    Index expected_id = -1;
    if (mode == SelectMode::GeometryVertex) {
        point = gp_Pnt(10.0, 10.0, 10.0);
        const auto& vertices = scene.geometry.index.type_maps[GeometrySubshapeIndex::typeIndex(TopAbs_VERTEX)];
        for (int local = 1; local <= vertices.Extent(); ++local) {
            if (BRep_Tool::Pnt(TopoDS::Vertex(vertices.FindKey(local))).Distance(point) < 1.0e-9)
                expected_id = scene.geometry.index.vertexGlobalId(local);
        }
    } else if (mode == SelectMode::GeometryEdge) {
        point = gp_Pnt(5.0, 10.0, 10.0);
        const auto& edges = scene.geometry.index.type_maps[GeometrySubshapeIndex::typeIndex(TopAbs_EDGE)];
        for (int local = 1; local <= edges.Extent(); ++local) {
            BRepAdaptor_Curve curve(TopoDS::Edge(edges.FindKey(local)));
            if (curve.Value((curve.FirstParameter() + curve.LastParameter()) * 0.5).Distance(point) < 1.0e-9)
                expected_id = scene.geometry.index.edgeGlobalId(local);
        }
    } else if (mode == SelectMode::GeometryFace) {
        const auto& faces = scene.geometry.index.type_maps[GeometrySubshapeIndex::typeIndex(TopAbs_FACE)];
        expected_id = scene.geometry.index.faceGlobalId(faces.FindIndex(box.TopFace()));
    } else {
        expected_id = scene.geometry.index.solidGlobalId(1);
    }
    REQUIRE(expected_id >= 0);
    const std::vector<Index> expected { expected_id };
    const auto display = scene.project(point);
    selection.select(display[0], display[1]);
    REQUIRE(selection.getSelection()->ids == expected);
    checkTriangulationsUnchanged(original_caches);
    checkTriangulationsUnchanged(snapshot_caches);

    // 宿主在切档前清空选择；重新建立的 Picker 必须仍能解析点、边、面和体。
    selection.clearSelection();
    auto* highlight_data = vtkPartitionedDataSet::SafeDownCast(
        highlight->GetMapper()->GetInputDataObject(0, 0));
    REQUIRE(highlight_data != nullptr);
    CHECK(highlight_data->GetNumberOfCells() == 0);
    scene.manager.setSurfacePrecision(3 - initial_precision);
    CHECK(selection.getSelection()->ids.empty());
    scene.window->Render();
    selection.select(display[0], display[1], SelectOp::Append);
    REQUIRE(selection.getSelection()->ids == expected);
    checkRepick(selection, display, expected);
    CHECK(highlight_data->GetNumberOfCells() > 0);
    checkTriangulationsUnchanged(original_caches);
    checkTriangulationsUnchanged(snapshot_caches);
}

TEST_CASE("CAD hidden geometry restores picking after precision rebuild", "[geometry][precision][render]")
{
    const bool component_hidden = GENERATE(false, true);
    GeometryScene scene(BRepPrimAPI_MakeSphere(10.0).Shape());
    scene.load();
    vtkNew<vtkActor> highlight;
    highlight->PickableOff();
    scene.renderer->AddActor(highlight);
    GeometrySelectManager selection(*scene.renderer, *highlight, scene.manager.op());
    selection.setSelectMode(SelectMode::GeometryFace);
    scene.lookDownZ(gp_Pnt(0.0, 0.0, 0.0));
    const auto display = scene.project(gp_Pnt(3.0, 2.0, std::sqrt(87.0)));
    const std::vector<Index> selected_ids { scene.geometry.index.faceGlobalId(1) };
    selection.select(display[0], display[1]);
    REQUIRE(selection.getSelection()->ids == selected_ids);

    // 分别覆盖全局 Hidden 样式和对象树组件可见性开关。
    const std::array<GeometryRenderStyle, 2> restored_styles {
        GeometryRenderStyle::Surface, GeometryRenderStyle::SurfaceWithEdges
    };
    int precision = 3;
    for (const auto style : restored_styles) {
        selection.clearSelection();
        if (component_hidden)
            scene.manager.setVisibility(kComponentId, false);
        else
            scene.manager.setCurrentRenderStyle(GeometryRenderStyle::Hidden);
        scene.manager.setSurfacePrecision(precision);
        CHECK_FALSE(scene.actor()->isVisible());
        CHECK(selection.getSelection()->ids.empty());

        scene.manager.setVisibility(kComponentId, true);
        scene.manager.setCurrentRenderStyle(style);
        scene.window->Render();
        REQUIRE(scene.actor()->isVisible());
        selection.select(display[0], display[1], SelectOp::Append);
        REQUIRE(selection.getSelection()->ids == selected_ids);
        checkRepick(selection, display, selected_ids);
        precision = 0;
    }
}

TEST_CASE("CAD batch precision failure keeps partial success and retries unfinished components", "[geometry][precision][failure][render]")
{
    const int fail_on = GENERATE(1, 2);
    GeometryScene scene(BRepPrimAPI_MakeSphere(10.0).Shape());
    const auto caches = captureTriangulations(*scene.geometry.rootShape);
    scene.load();
    GeometryData later;
    later.setRootShape(BRepPrimAPI_MakeSphere(gp_Pnt(30.0, 0.0, 0.0), 10.0).Shape());
    later.ensureIndexBuilt(scene.registry);
    scene.manager.loadGeometry({ *later.rootShape, kComponentId + 1, &later.index });
    scene.manager.setCurrentRenderStyle(GeometryRenderStyle::Transparent25);
    std::array<vtkIdType, 2> triangle_counts;
    std::array<vtkSmartPointer<vtkPolyData>, 2> polygons;
    for (int i = 0; i < 2; ++i) {
        polygons[i] = scene.polygons(kComponentId + i);
        triangle_counts[i] = polygons[i]->GetNumberOfPolys();
    }

    vtkNew<vtkActor> highlight;
    highlight->PickableOff();
    scene.renderer->AddActor(highlight);
    GeometrySelectManager selection(*scene.renderer, *highlight, scene.manager.op());
    selection.setSelectMode(SelectMode::GeometryFace);
    scene.lookDownZ(gp_Pnt(0.0, 0.0, 0.0));
    const auto display = scene.project(gp_Pnt(3.0, 2.0, std::sqrt(87.0)));
    const std::vector<Index> selected { scene.geometry.index.faceGlobalId(1) };
    selection.select(display[0], display[1]);
    REQUIRE(selection.getSelection()->ids == selected);
    selection.clearSelection();

    SurfacePrecisionFailure failure(fail_on);
    REQUIRE_THROWS_AS(scene.manager.setSurfacePrecision(3), std::runtime_error);
    CHECK(failure.rebuilds() == fail_on);
    CHECK(scene.manager.getSurfacePrecision() == 3);
    CHECK(selection.getSelection()->ids.empty());
    std::array<bool, 2> completed {};
    std::array<vtkMTimeType, 2> completed_times {};
    int completed_count = 0;
    for (int i = 0; i < 2; ++i) {
        const auto actor = scene.actor(kComponentId + i);
        GeometryActorSelectOp op(actor);
        completed[i] = actor->getSurfacePrecision() == 3;
        completed_times[i] = scene.polygons(kComponentId + i)->GetMTime();
        completed_count += completed[i];
        CHECK(scene.polygons(kComponentId + i) == polygons[i]);
        CHECK(scene.manager.op().getComponentId(&op.getPolyActor()) == kComponentId + i);
        if (completed[i])
            CHECK(scene.polygons(kComponentId + i)->GetNumberOfPolys() > triangle_counts[i]);
        else
            CHECK(scene.polygons(kComponentId + i)->GetNumberOfPolys() == triangle_counts[i]);
    }
    REQUIRE(completed_count == fail_on - 1);
    checkTriangulationsUnchanged(caches);

    // 失败后新导入仍使用窗口目标值，同档重试仅处理尚未完成的组件。
    scene.manager.loadGeometry({ *later.rootShape, kComponentId + 2, &later.index });
    CHECK(scene.actor(kComponentId + 2)->getSurfacePrecision() == 3);
    const int rebuilds_before_retry = failure.rebuilds();
    REQUIRE_NOTHROW(scene.manager.setSurfacePrecision(3));
    CHECK(failure.rebuilds() == rebuilds_before_retry + 2 - completed_count);
    for (int i = 0; i < 2; ++i) {
        const auto actor = scene.actor(kComponentId + i);
        GeometryActorSelectOp op(actor);
        CHECK(actor->getSurfacePrecision() == 3);
        CHECK(actor->getRenderStyle() == GeometryRenderStyle::Transparent25);
        CHECK(op.getPolyActor().GetProperty()->GetOpacity() == Catch::Approx(0.25));
        CHECK(scene.polygons(kComponentId + i)->GetNumberOfPolys() > triangle_counts[i]);
        if (completed[i])
            CHECK(scene.polygons(kComponentId + i)->GetMTime() == completed_times[i]);
    }
    scene.window->Render();
    selection.select(display[0], display[1], SelectOp::Append);
    REQUIRE(selection.getSelection()->ids == selected);
    checkRepick(selection, display, selected);
}

TEST_CASE("CAD precision refresh reuses diagnostic results and updates overlays from the current mesh", "[geometry][precision][diagnostics]")
{
    const bool edge_category = GENERATE(false, true);
    const auto category = edge_category
        ? GeometryTopologyDiagnosticCategory::SmallEdge
        : GeometryTopologyDiagnosticCategory::SmallFace;
    const double opacity = edge_category ? 1.0 : 0.4;
    const double marker_size = edge_category ? 10.0 : 12.0;
    GeometryScene scene(BRepPrimAPI_MakeSphere(10.0).Shape());
    scene.load();
    scene.manager.setTopologyDiagnosticSmallEdgeLength(2000.0);
    scene.manager.setTopologyDiagnosticSmallFaceArea(2000.0);
    scene.manager.setTopologyDiagnosticCategoryEnabled(static_cast<int>(category), true);
    vtkActor* overlay = nullptr;
    vtkPolyDataMapper* wireframe_mapper = nullptr;
    vtkPolyData* marker_data = nullptr;
    auto* actors = scene.renderer->GetActors();
    actors->InitTraversal();
    while (auto* actor = actors->GetNextActor()) {
        auto* mapper = vtkPolyDataMapper::SafeDownCast(actor->GetMapper());
        if (!mapper || !actor->GetVisibility())
            continue;
        if (actor->GetPickable()) {
            if (actor->GetProperty()->GetLineWidth() == 2.0
                && actor->GetProperty()->GetPointSize() == 6.0)
                wireframe_mapper = mapper;
            continue;
        }
        if (actor->GetProperty()->GetOpacity() == opacity
            && (!edge_category || actor->GetProperty()->GetLineWidth() == 4.0))
            overlay = actor;
        if (actor->GetProperty()->GetPointSize() == marker_size)
            marker_data = mapper->GetInput();
    }
    REQUIRE(overlay != nullptr);
    REQUIRE(wireframe_mapper != nullptr);
    REQUIRE(marker_data != nullptr);
    const auto marker_count = marker_data->GetNumberOfPoints();
    REQUIRE(marker_count > 0);
    auto* marker_points = marker_data->GetPoints();
    const int actor_count = actors->GetNumberOfItems();
    auto* overlay_data = vtkPolyDataMapper::SafeDownCast(overlay->GetMapper())->GetInput();
    std::array<vtkIdType, 4> connectivity_counts {};
    SurfacePrecisionFailure observations(1000);
    for (int precision : { 0, 3, 1, 2 }) {
        scene.manager.setSurfacePrecision(precision);
        wireframe_mapper->Update();
        auto* current = edge_category ? wireframe_mapper->GetInput() : scene.polygons();
        auto* current_cells = edge_category ? current->GetLines() : current->GetPolys();
        auto* overlay_cells = edge_category ? overlay_data->GetLines() : overlay_data->GetPolys();
        CHECK(overlay->GetVisibility());
        CHECK(overlay->GetProperty()->GetOpacity() == Catch::Approx(opacity));
        CHECK(overlay_cells->GetNumberOfCells() == current_cells->GetNumberOfCells());
        CHECK(overlay_cells->GetNumberOfConnectivityIds() == current_cells->GetNumberOfConnectivityIds());
        CHECK(overlay_cells->GetNumberOfCells() > 0);
        connectivity_counts[precision] = overlay_cells->GetNumberOfConnectivityIds();
        CHECK(marker_data->GetNumberOfPoints() == marker_count);
        CHECK(marker_data->GetPoints() == marker_points);
        CHECK(actors->GetNumberOfItems() == actor_count);
        CHECK(observations.diagnosticLogs() == 0);
    }
    // 曲边的折线顶点数与曲面的三角化均随精度变化，叠加必须使用新网格。
    CHECK(connectivity_counts[3] > connectivity_counts[0]);
    scene.manager.setTopologyDiagnosticCategoryEnabled(static_cast<int>(category), false);
    CHECK_FALSE(overlay->GetVisibility());
    scene.manager.setSurfacePrecision(0);
    CHECK_FALSE(overlay->GetVisibility());
    scene.manager.setTopologyDiagnosticCategoryEnabled(static_cast<int>(category), true);
    CHECK(overlay->GetVisibility());
    auto* restored_cells = edge_category ? overlay_data->GetLines() : overlay_data->GetPolys();
    CHECK(restored_cells->GetNumberOfConnectivityIds() == connectivity_counts[0]);
}

TEST_CASE("CAD display update failure can retry the same target after partial installation", "[geometry][precision][failure][render]")
{
    const int fail_on = GENERATE(1, 2);
    const bool fail_in_diagnostics = GENERATE(false, true);
    GeometryScene scene(BRepPrimAPI_MakeSphere(10.0).Shape());
    scene.load();
    GeometryData later;
    later.setRootShape(BRepPrimAPI_MakeSphere(gp_Pnt(30, 0, 0), 10.0).Shape());
    later.ensureIndexBuilt(scene.registry);
    scene.manager.loadGeometry({ *later.rootShape, kComponentId + 1, &later.index });
    scene.manager.setTopologyDiagnosticSmallFaceArea(2000);
    scene.manager.setTopologyDiagnosticCategoryEnabled(static_cast<int>(GeometryTopologyDiagnosticCategory::SmallFace), true);

    std::vector<vtkPolyData*> targets;
    if (fail_in_diagnostics) {
        auto* actors = scene.renderer->GetActors();
        actors->InitTraversal();
        while (auto* actor = actors->GetNextActor()) {
            if (!actor->GetPickable() && actor->GetVisibility()
                && actor->GetProperty()->GetOpacity() == 0.4) {
                auto* mapper = vtkPolyDataMapper::SafeDownCast(actor->GetMapper());
                if (mapper)
                    targets.push_back(mapper->GetInput());
            }
        }
    } else {
        targets = { scene.polygons(), scene.polygons(kComponentId + 1) };
    }
    REQUIRE(targets.size() == 2);
    struct FailureState {
        int fail_on;
        bool fired = false;
        std::unordered_set<vtkObject*> visited;
    } state { fail_on };
    vtkNew<vtkCallbackCommand> callback;
    callback->SetClientData(&state);
    callback->SetCallback([](vtkObject* object, unsigned long, void* client, void*) {
        auto& state = *static_cast<FailureState*>(client);
        if (!state.fired && state.visited.insert(object).second && state.visited.size() == static_cast<size_t>(state.fail_on)) {
            state.fired = true;
            throw std::runtime_error("injected display update failure");
        }
    });
    std::vector<unsigned long> observers;
    for (auto* target : targets)
        observers.push_back(target->AddObserver(vtkCommand::ModifiedEvent, callback));
    CHECK_THROWS_AS(scene.manager.setSurfacePrecision(3), std::runtime_error);
    for (size_t i = 0; i < targets.size(); ++i)
        targets[i]->RemoveObserver(observers[i]);
    REQUIRE(state.fired);
    CHECK(scene.manager.getSurfacePrecision() == 3);
    int completed_count = 0;
    for (int i = 0; i < 2; ++i)
        completed_count += scene.actor(kComponentId + i)->getSurfacePrecision() == 3;
    CHECK(completed_count == fail_on - 1);

    REQUIRE_NOTHROW(scene.manager.setSurfacePrecision(3));
    for (int i = 0; i < 2; ++i) {
        CHECK(scene.actor(kComponentId + i)->getSurfacePrecision() == 3);
        CHECK(scene.polygons(kComponentId + i)->GetNumberOfPolys() > 0);
    }
    // 恢复到先前已成功过的档位也必须执行真实重建。
    REQUIRE_NOTHROW(scene.manager.setSurfacePrecision(1));
    for (int i = 0; i < 2; ++i)
        CHECK(scene.actor(kComponentId + i)->getSurfacePrecision() == 1);
}

TEST_CASE("CAD same precision retry restores registration after pick list failure", "[geometry][precision][failure][render]")
{
    const bool persistent_failure = GENERATE(false, true);
    GeometryScene scene(BRepPrimAPI_MakeSphere(10.0).Shape());
    vtkNew<vtkPropCollection> pick_list;
    scene.manager.op().observePickList(pick_list);
    scene.load();
    REQUIRE(pick_list->GetNumberOfItems() == 1);

    vtkNew<vtkActor> highlight;
    highlight->PickableOff();
    scene.renderer->AddActor(highlight);
    GeometrySelectManager selection(*scene.renderer, *highlight, scene.manager.op());
    selection.setSelectMode(SelectMode::GeometryFace);
    scene.lookDownZ(gp_Pnt(0.0, 0.0, 0.0));
    const auto display = scene.project(gp_Pnt(3.0, 2.0, std::sqrt(87.0)));
    const std::vector<Index> expected { scene.geometry.index.faceGlobalId(1) };
    selection.select(display[0], display[1]);
    REQUIRE(selection.getSelection()->ids == expected);
    selection.clearSelection();

    struct RegistrationFailure {
        bool persistent;
        bool enabled = true;
        int failures = 0;
    } state { persistent_failure };
    vtkNew<vtkCallbackCommand> callback;
    callback->SetClientData(&state);
    callback->SetCallback([](vtkObject* object, unsigned long, void* client, void*) {
        auto& state = *static_cast<RegistrationFailure*>(client);
        auto* list = vtkPropCollection::SafeDownCast(object);
        // AddItem 已安装元素后触发 Modified；清理时的 RemoveItem 可以正常完成。
        if (state.enabled && list && list->GetNumberOfItems() > 0) {
            ++state.failures;
            state.enabled = state.persistent;
            throw std::runtime_error("injected pick list registration failure");
        }
    });
    const auto observer = pick_list->AddObserver(vtkCommand::ModifiedEvent, callback);
    SurfacePrecisionFailure observations(1000);
    CHECK_THROWS_AS(scene.manager.setSurfacePrecision(3), std::runtime_error);
    pick_list->RemoveObserver(observer);
    REQUIRE(state.failures > 0);
    REQUIRE(scene.actor()->getSurfacePrecision() == 3);
    CHECK(scene.manager.getSurfacePrecision() == 3);
    CHECK(observations.rebuilds() == 1);
    const auto installed_mesh_time = scene.polygons()->GetMTime();

    REQUIRE_NOTHROW(scene.manager.setSurfacePrecision(3));
    CHECK(scene.polygons()->GetMTime() == installed_mesh_time);
    CHECK(observations.rebuilds() == 1);
    GeometryActorSelectOp op(scene.actor());
    CHECK(pick_list->GetNumberOfItems() == 1);
    CHECK(pick_list->IsItemPresent(&op.getPolyActor()) == 1);
    CHECK(scene.manager.op().getComponentId(&op.getPolyActor()) == kComponentId);
    CHECK(scene.manager.op().getComponentIdByShapeId(op.getShapeId()) == kComponentId);
    CHECK(scene.manager.op().getAllComponentIds() == std::vector<Index> { kComponentId });
    scene.window->Render();
    selection.select(display[0], display[1], SelectOp::Append);
    REQUIRE(selection.getSelection()->ids == expected);
    checkRepick(selection, display, expected);
}
