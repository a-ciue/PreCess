#include "GeometryActor.h"
#include "GeometryActorManager.h"
#include "GeometryActorSelectOp.h"
#include "GeometryData.h"
#include "GeometryDataVtk.h"
#include "GeometryRegistry.h"
#include "GeometrySelectManager.h"

#include <BRepAdaptor_Curve.hxx>
#include <BRepBuilderAPI_Copy.hxx>
#include <BRepBuilderAPI_MakeEdge.hxx>
#include <BRepBuilderAPI_MakeFace.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRep_Builder.hxx>
#include <BRep_Tool.hxx>
#include <Geom_BSplineSurface.hxx>
#include <IVtkOCC_Shape.hxx>
#include <IVtkTools_ShapeDataSource.hxx>
#include <IVtkTools_SubPolyDataFilter.hxx>
#include <IVtkVTK_ShapeData.hxx>
#include <NCollection_Array1.hxx>
#include <NCollection_Array2.hxx>
#include <Prs3d_Drawer.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Vertex.hxx>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <gp_Pln.hxx>
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkDataArray.h>
#include <vtkIdList.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPointData.h>
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

std::vector<vtkIdType> topologyCells(vtkPolyData& data, const OccShapeHandle& shape, bool faces)
{
    auto* ids = data.GetCellData()->GetArray(IVtkVTK_ShapeData::ARRNAME_SUBSHAPE_IDS());
    REQUIRE(ids != nullptr);
    std::vector<vtkIdType> cells;
    for (vtkIdType cell = 0; cell < data.GetNumberOfCells(); ++cell) {
        const auto sub_id = static_cast<IVtk_IdType>(ids->GetComponent(cell, 0));
        const auto& subshape = shape->GetSubShape(sub_id);
        REQUIRE_FALSE(subshape.IsNull());
        const auto type = subshape.ShapeType();
        if (faces ? data.GetCellType(cell) == VTK_TRIANGLE && type == TopAbs_FACE
                  : type == TopAbs_EDGE || type == TopAbs_VERTEX)
            cells.push_back(cell);
    }
    return cells;
}

void checkTopologyCells(vtkPolyData& actual, vtkPolyData& source,
    const std::vector<vtkIdType>& expected, bool faces)
{
    REQUIRE(actual.GetNumberOfCells() == static_cast<vtkIdType>(expected.size()));
    // 按源拓扑归属独立计算期望，防止把面等参线或面归属的重复边当作拓扑边。
    for (const auto* name : { IVtkVTK_ShapeData::ARRNAME_SUBSHAPE_IDS(),
             IVtkVTK_ShapeData::ARRNAME_MESH_TYPES() }) {
        auto* actual_ids = actual.GetCellData()->GetArray(name);
        auto* source_ids = source.GetCellData()->GetArray(name);
        REQUIRE(actual_ids != nullptr);
        REQUIRE(source_ids != nullptr);
        REQUIRE(actual_ids->GetNumberOfTuples() == actual.GetNumberOfCells());
        for (vtkIdType cell = 0; cell < actual.GetNumberOfCells(); ++cell)
            CHECK(actual_ids->GetComponent(cell, 0) == source_ids->GetComponent(expected[cell], 0));
    }
    auto* actual_normals = actual.GetPointData()->GetNormals();
    auto* source_normals = source.GetPointData()->GetNormals();
    if (faces) {
        REQUIRE(actual_normals != nullptr);
        REQUIRE(source_normals != nullptr);
        REQUIRE(actual_normals->GetNumberOfComponents() == 3);
    }
    vtkNew<vtkIdList> actual_points;
    vtkNew<vtkIdList> source_points;
    for (vtkIdType cell = 0; cell < actual.GetNumberOfCells(); ++cell) {
        REQUIRE(actual.GetCellType(cell) == source.GetCellType(expected[cell]));
        actual.GetCellPoints(cell, actual_points);
        source.GetCellPoints(expected[cell], source_points);
        REQUIRE(actual_points->GetNumberOfIds() == source_points->GetNumberOfIds());
        for (vtkIdType point = 0; point < actual_points->GetNumberOfIds(); ++point) {
            const auto actual_id = actual_points->GetId(point);
            const auto source_id = source_points->GetId(point);
            double actual_xyz[3];
            double source_xyz[3];
            actual.GetPoint(actual_id, actual_xyz);
            source.GetPoint(source_id, source_xyz);
            for (int axis = 0; axis < 3; ++axis) {
                CHECK(actual_xyz[axis] == Catch::Approx(source_xyz[axis]).margin(1.0e-8));
                if (faces)
                    CHECK(actual_normals->GetComponent(actual_id, axis)
                        == Catch::Approx(source_normals->GetComponent(source_id, axis)).margin(1.0e-6));
            }
        }
    }
}

} // namespace

TEST_CASE("CAD surface precision rebuilds in both directions and applies to later imports", "[geometry][precision]")
{
    // 独立形状核对基线标准参数生成的默认输出。
    const OccShapeHandle reference_occ = new IVtkOCC_Shape(BRepPrimAPI_MakeSphere(10.0).Shape());
    vtkNew<IVtkTools_ShapeDataSource> reference_source;
    reference_source->SetShape(reference_occ);
    reference_source->Update();

    GeometryScene scene(BRepPrimAPI_MakeSphere(10.0).Shape());
    scene.load();
    CHECK(scene.polygons()->GetNumberOfPolys() == reference_source->GetOutput()->GetNumberOfPolys());
    CHECK(scene.lines()->GetNumberOfCells()
        == static_cast<vtkIdType>(topologyCells(*reference_source->GetOutput(), reference_occ, false).size()));
    auto* original_polygons = scene.polygons();
    auto* original_actor = &GeometryActorSelectOp(scene.actor()).getPolyActor();
    std::array<vtkIdType, 4> counts {};
    for (int precision = 0; precision < 4; ++precision) {
        scene.manager.setSurfacePrecision(precision);
        counts[precision] = scene.polygons()->GetNumberOfPolys();
        CHECK(scene.polygons() == original_polygons);
        CHECK(&GeometryActorSelectOp(scene.actor()).getPolyActor() == original_actor);
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
    CHECK(scene.polygons()->GetNumberOfPolys() == counts[3]);
}

TEST_CASE("CAD mesh classification preserves topology cells identities and normals", "[geometry][precision]")
{
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    builder.Add(compound, BRepPrimAPI_MakeBox(10.0, 10.0, 10.0).Shape());
    builder.Add(compound, BRepPrimAPI_MakeSphere(gp_Pnt(30.0, 0.0, 0.0), 10.0).Shape());
    builder.Add(compound, BRepBuilderAPI_MakeFace(gp_Pln(gp_Pnt(60.0, 0.0, 0.0), gp_Dir(0.0, 0.0, 1.0)), -5.0, 5.0, -5.0, 5.0).Shape());
    builder.Add(compound, BRepBuilderAPI_MakeEdge(gp_Pnt(80.0, 0.0, 0.0), gp_Pnt(90.0, 0.0, 0.0)).Shape());
    builder.Add(compound, BRepBuilderAPI_MakeVertex(gp_Pnt(100.0, 0.0, 0.0)).Shape());
    GeometryScene scene(compound);
    scene.load();
    auto* original_lines = scene.lines();
    auto* original_polygons = scene.polygons();
    auto* original_mapper = GeometryActorSelectOp(scene.actor()).getPolyActor().GetMapper();
    for (int precision : { 1, 0, 3, 0 }) {
        CAPTURE(precision);
        // 独立 OCCT 输出作参照，不复用被测分类器。
        BRepBuilderAPI_Copy reference_copy(compound, false, false);
        REQUIRE(reference_copy.IsDone());
        const OccShapeHandle reference = new IVtkOCC_Shape(reference_copy.Shape());
        const auto& settings = surface_precision::kLevels[static_cast<size_t>(precision)];
        reference->Attributes()->SetDeviationCoefficient(settings.deviation_coefficient);
        reference->Attributes()->SetDeviationAngle(settings.angle_degrees * std::acos(-1.0) / 180.0);
        vtkNew<IVtkTools_ShapeDataSource> source;
        source->SetShape(reference);
        source->Update();
        auto* source_data = source->GetOutput();
        const auto expected_lines = topologyCells(*source_data, reference, false);
        const auto expected_faces = topologyCells(*source_data, reference, true);
        REQUIRE_FALSE(expected_lines.empty());
        REQUIRE_FALSE(expected_faces.empty());
        REQUIRE(source_data->GetNumberOfCells()
            > static_cast<vtkIdType>(expected_lines.size() + expected_faces.size()));

        std::array<bool, MT_SeamEdge + 1> mesh_types {};
        auto* types = source_data->GetCellData()->GetArray(IVtkVTK_ShapeData::ARRNAME_MESH_TYPES());
        REQUIRE(types != nullptr);
        for (const auto cell : expected_lines) {
            const int type = static_cast<int>(types->GetComponent(cell, 0));
            REQUIRE(type >= 0);
            REQUIRE(type <= MT_SeamEdge);
            mesh_types[static_cast<size_t>(type)] = true;
        }
        CHECK(mesh_types[MT_FreeVertex]);
        CHECK(mesh_types[MT_SharedVertex]);
        CHECK(mesh_types[MT_FreeEdge]);
        CHECK(mesh_types[MT_BoundaryEdge]);
        CHECK(mesh_types[MT_SharedEdge]);
        CHECK(mesh_types[MT_SeamEdge]);

        scene.manager.setSurfacePrecision(precision);
        CHECK(scene.lines() == original_lines);
        CHECK(scene.polygons() == original_polygons);
        CHECK(GeometryActorSelectOp(scene.actor()).getPolyActor().GetMapper() == original_mapper);
        checkTopologyCells(*scene.lines(), *source_data, expected_lines, false);
        checkTopologyCells(*scene.polygons(), *source_data, expected_faces, true);
        // 远处的自由边与孤立顶点只属于线数据，不能扩大面的取景边界。
        const auto* face_bounds = scene.polygons()->GetBounds();
        CHECK(face_bounds[0] == Catch::Approx(0.0));
        CHECK(face_bounds[1] == Catch::Approx(65.0));
        CHECK(scene.lines()->GetBounds()[1] == Catch::Approx(100.0));
    }
}

TEST_CASE("CAD empty and faceless geometry retain compact bounds across precision levels", "[geometry][precision]")
{
    const int geometry_kind = GENERATE(0, 1, 2);
    BRep_Builder builder;
    TopoDS_Compound compound;
    builder.MakeCompound(compound);
    if (geometry_kind == 1)
        builder.Add(compound, BRepBuilderAPI_MakeEdge(gp_Pnt(80.0, 2.0, 3.0), gp_Pnt(90.0, 2.0, 3.0)).Shape());
    else if (geometry_kind == 2)
        builder.Add(compound, BRepBuilderAPI_MakeVertex(gp_Pnt(100.0, 2.0, 3.0)).Shape());
    GeometryScene scene(compound);
    REQUIRE_NOTHROW(scene.load());
    auto* original_lines = scene.lines();
    auto* original_polygons = scene.polygons();
    vtkPolyDataMapper* line_mapper = nullptr;
    auto* actors = scene.renderer->GetActors();
    actors->InitTraversal();
    while (auto* actor = actors->GetNextActor()) {
        auto* mapper = vtkPolyDataMapper::SafeDownCast(actor->GetMapper());
        if (mapper && mapper->GetInput() == original_lines)
            line_mapper = mapper;
    }
    REQUIRE(line_mapper != nullptr);
    for (int precision : { 0, 1, 2, 3, 0 }) {
        CAPTURE(geometry_kind, precision);
        REQUIRE_NOTHROW(scene.manager.setSurfacePrecision(precision));
        REQUIRE(scene.lines() == original_lines);
        REQUIRE(scene.polygons() == original_polygons);
        CHECK(scene.polygons()->GetNumberOfCells() == 0);
        CHECK(scene.polygons()->GetNumberOfPoints() == 0);
        if (geometry_kind == 0) {
            CHECK(scene.lines()->GetNumberOfCells() == 0);
            CHECK(scene.lines()->GetNumberOfPoints() == 0);
        } else {
            CHECK(scene.lines()->GetNumberOfVerts() == (geometry_kind == 1 ? 2 : 1));
            CHECK(scene.lines()->GetNumberOfLines() == (geometry_kind == 1 ? 1 : 0));
            line_mapper->Update();
            const auto* bounds = line_mapper->GetBounds();
            CHECK(bounds[0] == Catch::Approx(geometry_kind == 1 ? 80.0 : 100.0));
            CHECK(bounds[1] == Catch::Approx(geometry_kind == 1 ? 90.0 : 100.0));
            CHECK(bounds[2] == Catch::Approx(2.0));
            CHECK(bounds[3] == Catch::Approx(2.0));
            CHECK(bounds[4] == Catch::Approx(3.0));
            CHECK(bounds[5] == Catch::Approx(3.0));
        }
    }
}

TEST_CASE("CAD nonstandard initial precision supports registered picking before and after loading", "[geometry][precision][render]")
{
    const int initial_precision = GENERATE(0, 3);
    const bool enable_before_load = GENERATE(false, true);
    const auto mode = GENERATE(SelectMode::GeometryVertex, SelectMode::GeometryEdge,
        SelectMode::GeometryFace, SelectMode::GeometrySolid);
    BRepPrimAPI_MakeBox box(10.0, 10.0, 10.0);
    GeometryScene scene(box.Shape());
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

    // 宿主在切档前清空选择；既有 Picker 缓存继续解析点、边、面和体。
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
}

TEST_CASE("CAD spline face and curved edge picking stay stable across display precision and mode changes", "[geometry][precision][render]")
{
    const int initial_precision = GENERATE(0, 3);
    NCollection_Array2<gp_Pnt> poles(1, 4, 1, 4);
    for (int u = 0; u < 4; ++u) {
        for (int v = 0; v < 4; ++v) {
            const double z = (u == 1 || u == 2 ? 5.0 : 0.0)
                + (v == 1 || v == 2 ? 3.0 : 0.0);
            poles.SetValue(u + 1, v + 1, gp_Pnt(10.0 * u / 3.0, 10.0 * v / 3.0, z));
        }
    }
    NCollection_Array1<double> knots(1, 2);
    knots.SetValue(1, 0.0);
    knots.SetValue(2, 1.0);
    NCollection_Array1<int> multiplicities(1, 2);
    multiplicities.Init(4);
    const Handle(Geom_BSplineSurface) surface = new Geom_BSplineSurface(
        poles, knots, knots, multiplicities, multiplicities, 3, 3);
    GeometryScene scene(BRepBuilderAPI_MakeFace(surface, 1.0e-7).Shape());
    scene.manager.setSurfacePrecision(initial_precision);
    vtkNew<vtkActor> highlight;
    highlight->PickableOff();
    scene.renderer->AddActor(highlight);
    // 使用窗口的真实顺序：先创建 Picker，再按标准网格初始化全部模式，最后显示请求档位。
    GeometrySelectManager selection(*scene.renderer, *highlight, scene.manager.op());
    scene.load();
    scene.lookDownZ(gp_Pnt(5.0, 5.0, 0.0));

    const std::array<SelectMode, 3> modes {
        SelectMode::GeometryFace, SelectMode::GeometryEdge, SelectMode::GeometryVertex
    };
    const std::array<gp_Pnt, 3> points {
        surface->Value(0.5, 0.5), surface->Value(0.5, 0.0), surface->Value(0.0, 0.0)
    };
    std::array<Index, 3> expected_ids { scene.geometry.index.faceGlobalId(1), -1, -1 };
    const auto& edges = scene.geometry.index.type_maps[GeometrySubshapeIndex::typeIndex(TopAbs_EDGE)];
    for (int local = 1; local <= edges.Extent(); ++local) {
        BRepAdaptor_Curve curve(TopoDS::Edge(edges.FindKey(local)));
        if (curve.Value((curve.FirstParameter() + curve.LastParameter()) * 0.5).Distance(points[1]) < 1.0e-7)
            expected_ids[1] = scene.geometry.index.edgeGlobalId(local);
    }
    const auto& vertices = scene.geometry.index.type_maps[GeometrySubshapeIndex::typeIndex(TopAbs_VERTEX)];
    for (int local = 1; local <= vertices.Extent(); ++local) {
        if (BRep_Tool::Pnt(TopoDS::Vertex(vertices.FindKey(local))).Distance(points[2]) < 1.0e-7)
            expected_ids[2] = scene.geometry.index.vertexGlobalId(local);
    }
    for (const auto id : expected_ids)
        REQUIRE(id >= 0);

    auto pick_samples = [&](size_t mode_index) {
        selection.setSelectMode(modes[mode_index]);
        const auto display = scene.project(points[mode_index]);
        selection.select(display[0], display[1]);
        REQUIRE(selection.getSelection()->ids == std::vector<Index> { expected_ids[mode_index] });
        std::vector<std::vector<Index>> samples;
        for (const int dx : { -2, 0, 2 }) {
            for (const int dy : { -2, 0, 2 }) {
                selection.clearSelection();
                selection.select(display[0] + dx, display[1] + dy);
                samples.push_back(selection.getSelection()->ids);
            }
        }
        selection.clearSelection();
        return samples;
    };
    std::array<std::vector<std::vector<Index>>, 3> initial_hits;
    for (size_t mode = 0; mode < modes.size(); ++mode)
        initial_hits[mode] = pick_samples(mode);

    for (const int precision : { 3, 0, 2, 1, 0 }) {
        CAPTURE(initial_precision, precision);
        selection.clearSelection();
        scene.manager.setSurfacePrecision(precision);
        scene.window->Render();
        // 反复切换模式，命中仍来自加载阶段建立的标准精度缓存。
        for (size_t mode = 0; mode < modes.size(); ++mode) {
            CAPTURE(mode);
            CHECK(pick_samples(mode) == initial_hits[mode]);
        }
    }
}

TEST_CASE("CAD hidden geometry restores picking after precision rebuild", "[geometry][precision][render]")
{
    const bool component_hidden = GENERATE(false, true);
    GeometryScene scene(BRepPrimAPI_MakeSphere(10.0).Shape());
    vtkNew<vtkActor> highlight;
    highlight->PickableOff();
    scene.renderer->AddActor(highlight);
    GeometrySelectManager selection(*scene.renderer, *highlight, scene.manager.op());
    scene.load();
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

        if (component_hidden)
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

TEST_CASE("CAD display styles remain unchanged across precision updates", "[geometry][precision]")
{
    struct ExpectedStyle {
        GeometryRenderStyle style;
        double opacity;
        bool lines;
        bool triangle_edges;
    };
    const std::array<ExpectedStyle, 7> expected_styles { {
        { GeometryRenderStyle::SurfaceWithEdges, 1.0, true, false },
        { GeometryRenderStyle::Surface, 1.0, false, false },
        { GeometryRenderStyle::Transparent75, 0.75, true, false },
        { GeometryRenderStyle::Transparent50, 0.50, true, false },
        { GeometryRenderStyle::Transparent25, 0.25, true, false },
        { GeometryRenderStyle::WireframeWithLines, 0.1, true, true },
        { GeometryRenderStyle::Wireframe, 0.001, true, false },
    } };
    GeometryScene scene(BRepPrimAPI_MakeSphere(10.0).Shape());
    scene.load();
    auto& polygons = GeometryActorSelectOp(scene.actor()).getPolyActor();
    vtkActor* lines = nullptr;
    auto* actors = scene.renderer->GetActors();
    actors->InitTraversal();
    while (auto* actor = actors->GetNextActor()) {
        if (actor->GetProperty()->GetLineWidth() == 2.0
            && actor->GetProperty()->GetPointSize() == 6.0)
            lines = actor;
    }
    REQUIRE(lines != nullptr);
    int precision = 3;
    for (const auto& expected : expected_styles) {
        CAPTURE(static_cast<int>(expected.style));
        scene.manager.setCurrentRenderStyle(expected.style);
        scene.manager.setSurfacePrecision(precision);
        CHECK(polygons.GetVisibility());
        CHECK(polygons.GetProperty()->GetRepresentation() == VTK_SURFACE);
        CHECK(polygons.GetProperty()->GetOpacity() == Catch::Approx(expected.opacity));
        CHECK(static_cast<bool>(polygons.GetProperty()->GetEdgeVisibility()) == expected.triangle_edges);
        CHECK(static_cast<bool>(lines->GetVisibility()) == expected.lines);
        scene.manager.setVisibility(kComponentId, false);
        CHECK_FALSE(polygons.GetVisibility());
        CHECK_FALSE(lines->GetVisibility());
        scene.manager.setVisibility(kComponentId, true);
        CHECK(polygons.GetVisibility());
        CHECK(static_cast<bool>(lines->GetVisibility()) == expected.lines);
        scene.manager.setCurrentRenderStyle(GeometryRenderStyle::Hidden);
        CHECK_FALSE(polygons.GetVisibility());
        CHECK_FALSE(lines->GetVisibility());
        precision = 3 - precision;
    }
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
