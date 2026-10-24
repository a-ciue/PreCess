#include "GeometryActor.h"
#include "GeometryActorManager.h"
#include "GeometryActorSelectOp.h"
#include "GeometryData.h"
#include "GeometryDataVtk.h"
#include "GeometryRegistry.h"
#include "GeometrySelectManager.h"
#include "SurfacePrecisionFailure.h"

#include <BRepMesh_IncrementalMesh.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Tool.hxx>
#include <IVtkOCC_Shape.hxx>
#include <IVtkTools_ShapeDataSource.hxx>
#include <IVtkTools_ShapePicker.hxx>
#include <IVtkTools_SubPolyDataFilter.hxx>
#include <Poly_Triangulation.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
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

    scene.manager.setSurfacePrecision(-1);
    scene.manager.setSurfacePrecision(4);
    CHECK(scene.manager.getSurfacePrecision() == 3);
    CHECK(scene.polygons()->GetNumberOfPolys() == counts[3]);
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
    selection.select(display[0], display[1]);
    const auto selected_ids = selection.getSelection()->ids;
    REQUIRE(selected_ids == std::vector<Index> { scene.geometry.index.faceGlobalId(1) });

    // 分别覆盖全局 Hidden 样式和对象树组件可见性开关。
    const std::array<GeometryRenderStyle, 2> restored_styles {
        GeometryRenderStyle::Surface, GeometryRenderStyle::SurfaceWithEdges
    };
    int precision = 3;
    for (const auto style : restored_styles) {
        if (component_hidden)
            scene.manager.setVisibility(kComponentId, false);
        else
            scene.manager.setCurrentRenderStyle(GeometryRenderStyle::Hidden);
        scene.manager.setSurfacePrecision(precision);
        CHECK_FALSE(scene.actor()->isVisible());
        CHECK(selection.getSelection()->ids == selected_ids);

        scene.manager.setVisibility(kComponentId, true);
        scene.manager.setCurrentRenderStyle(style);
        scene.window->Render();
        REQUIRE(scene.actor()->isVisible());

        // 恢复后真正重拾一次：若 Picker 仍处于禁用态，Toggle 会走空拾取清空；
        // 紧接着 Append 必须重新命中，避免把清空误判为恢复成功。
        checkRepick(selection, display, selected_ids);
        precision = 0;
    }
}

TEST_CASE("CAD batch precision failure keeps every component pickable and retries atomically", "[geometry][precision][failure][render]")
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

    std::array<vtkIdType, 2> triangle_counts;
    std::array<vtkSmartPointer<vtkPolyData>, 2> polygons;
    for (int i = 0; i < 2; ++i) {
        polygons[i] = scene.polygons(kComponentId + i);
        triangle_counts[i] = polygons[i]->GetNumberOfPolys();
    }
    const int actor_count = scene.renderer->GetActors()->GetNumberOfItems();
    SurfacePrecisionFailure failure(fail_on);
    REQUIRE_THROWS_AS(scene.manager.setSurfacePrecision(3), std::runtime_error);
    REQUIRE(failure.rebuilds() == fail_on); // 覆盖首个候选失败与已有候选后失败。
    checkTriangulationsUnchanged(caches);
    CHECK(scene.manager.getSurfacePrecision() == 1);
    CHECK(scene.renderer->GetActors()->GetNumberOfItems() == actor_count);
    for (int i = 0; i < 2; ++i) {
        const auto actor = scene.manager.getComponentActor(kComponentId + i);
        GeometryActorSelectOp op(actor);
        CHECK(scene.polygons(kComponentId + i) == polygons[i]);
        CHECK(scene.manager.op().getComponentId(&op.getPolyActor()) == kComponentId + i);
    }
    checkRepick(selection, display, selected);

    REQUIRE_NOTHROW(scene.manager.setSurfacePrecision(3));
    CHECK(scene.manager.getSurfacePrecision() == 3);
    CHECK(scene.renderer->GetActors()->GetNumberOfItems() == actor_count);
    for (int i = 0; i < 2; ++i) {
        const auto actor = scene.manager.getComponentActor(kComponentId + i);
        GeometryActorSelectOp op(actor);
        CHECK(actor->getRenderStyle() == GeometryRenderStyle::Transparent25);
        CHECK(op.getPolyActor().GetProperty()->GetOpacity() == Catch::Approx(0.25));
        CHECK(scene.polygons(kComponentId + i)->GetNumberOfPolys()
            > triangle_counts[i]);
        CHECK(scene.polygons(kComponentId + i) == polygons[i]);
    }
    CHECK(selection.getSelection()->ids == selected);
    CHECK(failure.diagnosticLogs() == 0);
    scene.window->Render();
    checkRepick(selection, display, selected);
    scene.manager.loadGeometry({ *later.rootShape, kComponentId + 2, &later.index });
    CHECK(scene.polygons(kComponentId + 2)->GetNumberOfPolys() == scene.polygons()->GetNumberOfPolys());
}

TEST_CASE("CAD display precision keeps independent picking and refreshes curved highlights", "[geometry][precision][render]")
{
    GeometryScene scene(BRepPrimAPI_MakeSphere(10.0).Shape());
    scene.load();
    scene.lookDownZ(gp_Pnt(0, 0, 0));
    const auto center = scene.project(gp_Pnt(3, 2, std::sqrt(87.0)));

    // 不向 Manager 注册此 Picker：切档若替换拾取 Shape，其缓存不会被补注册掩盖。
    vtkNew<IVtkTools_ShapePicker> picker;
    picker->SetRenderer(scene.renderer);
    picker->SetAreaSelection(false);
    GeometryActorSelectOp op(scene.actor());
    op.enableSelectionMode(picker, SelectMode::GeometryFace);
    REQUIRE(picker->Pick(center[0], center[1], 0, scene.renderer) > 0);
    const auto sub_id = picker->GetPickedSubShapesIds(kComponentId).First();
    auto* actor = &op.getPolyActor();
    const auto* mapper = actor->GetMapper();
    vtkNew<vtkPartitionedDataSet> data;
    GeometryFaceSelectorHighlight selection(*data, 0, op);
    selection.toggle(sub_id, scene.geometry.index.faceGlobalId(1), SelectOp::Append);
    const auto selected = selection.get().ids;
    for (int precision : { 0, 3, 1, 2 }) {
        scene.manager.setSurfacePrecision(precision);
        selection.enableHighlight();
        scene.window->Render();
        CHECK(&op.getPolyActor() == actor);
        CHECK(op.getPolyActor().GetMapper() == mapper);
        CHECK(selection.get().ids == selected);
        CHECK(data->GetNumberOfCells() == scene.polygons()->GetNumberOfCells());
        // 从中心到轮廓附近都实际拾取，确认固定标准拾取精度匹配各显示档位。
        for (double x : { 0.0, 3.0, 8.0, 9.8 }) {
            const auto point = scene.project(gp_Pnt(x, 0, std::sqrt(100.0 - x * x)));
            REQUIRE(picker->Pick(point[0], point[1], 0, scene.renderer) > 0);
            CHECK(picker->GetPickedSubShapesIds(kComponentId).First() == sub_id);
            IVtk_IdType picked_id;
            CHECK(op.resolvePickedSubshape(picker, kComponentId, SelectMode::GeometryFace, picked_id)
                == scene.geometry.index.faceGlobalId(1));
        }
    }
    op.removeFromPickers({ picker });
    CHECK(picker->Pick(center[0], center[1], 0, scene.renderer) == 0);
}

TEST_CASE("CAD commit failure restores current and previous display data before retry", "[geometry][precision][failure][render]")
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
    std::vector<vtkPolyData*> all_data;
    for (int i = 0; i < 2; ++i) {
        auto* polygons = scene.polygons(kComponentId + i);
        all_data.push_back(polygons);
        all_data.push_back(scene.lines(kComponentId + i));
        if (!fail_in_diagnostics)
            targets.push_back(polygons);
    }
    auto* actors = scene.renderer->GetActors();
    actors->InitTraversal();
    while (auto* actor = actors->GetNextActor()) {
        if (actor->GetPickable())
            continue;
        auto* mapper = vtkPolyDataMapper::SafeDownCast(actor->GetMapper());
        if (!mapper)
            continue;
        all_data.push_back(mapper->GetInput());
        if (fail_in_diagnostics && actor->GetVisibility() && actor->GetProperty()->GetOpacity() == 0.4)
            targets.push_back(mapper->GetInput());
    }
    REQUIRE(targets.size() == 2);
    std::vector<vtkSmartPointer<vtkPolyData>> previous;
    for (auto* data : all_data) {
        auto snapshot = vtkSmartPointer<vtkPolyData>::New();
        snapshot->ShallowCopy(data);
        previous.push_back(snapshot);
    }
    struct FailureState {
        int fail_on;
        bool fired = false;
        std::unordered_set<vtkObject*> visited;
    } state { fail_on };
    vtkNew<vtkCallbackCommand> callback;
    callback->SetClientData(&state);
    callback->SetCallback([](vtkObject* object, unsigned long, void* client, void*) {
        auto& state = *static_cast<FailureState*>(client);
        if (!state.fired && state.visited.insert(object).second && state.visited.size() == state.fail_on) {
            state.fired = true;
            throw std::runtime_error("injected display commit failure");
        }
    });
    std::vector<unsigned long> observers;
    for (auto* target : targets)
        observers.push_back(target->AddObserver(vtkCommand::ModifiedEvent, callback));
    REQUIRE_THROWS_AS(scene.manager.setSurfacePrecision(3), std::runtime_error);
    for (size_t i = 0; i < targets.size(); ++i)
        targets[i]->RemoveObserver(observers[i]);
    REQUIRE(state.fired);
    CHECK(scene.manager.getSurfacePrecision() == 1);
    for (size_t i = 0; i < all_data.size(); ++i) {
        CHECK(all_data[i]->GetPoints() == previous[i]->GetPoints());
        CHECK(all_data[i]->GetPolys() == previous[i]->GetPolys());
        CHECK(all_data[i]->GetLines() == previous[i]->GetLines());
        CHECK(all_data[i]->GetNumberOfCells() == previous[i]->GetNumberOfCells());
    }
    REQUIRE_NOTHROW(scene.manager.setSurfacePrecision(3));
    CHECK(scene.manager.getSurfacePrecision() == 3);
    CHECK(scene.polygons()->GetNumberOfCells() > previous.front()->GetNumberOfCells());
}
