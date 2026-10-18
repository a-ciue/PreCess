#include "MakeMeshDataVtk.h"
#include "MakeMeshData.h"

#include "FaceHideFilter.h"
#include "TestMeshActorWithData.h"
#include "MeshActorManager.h"
#include "MeshActorSelectOp.h"
#include "MeshPipelineData.h"
#include "renderStrategy/AttributeOperator.h"
#include "renderStrategy/AttriRenderStrategyScalar.h"
#include "renderStrategy/AttriRenderStrategyVector.h"
#include <catch2/catch_test_macros.hpp>
#include <vtkActor.h>
#include <vtkAlgorithmOutput.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkCellData.h>
#include <vtkDoubleArray.h>
#include <vtkHardwareSelector.h>
#include <vtkIdTypeArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPlane.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkPropCollection.h>
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkScalarBarActor.h>
#include <vtkSelection.h>
#include <vtkSmartPointer.h>
#include <vtkTrivialProducer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkWindowToImageFilter.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

//! @brief 面管线测试环境：加载演示网格，提供经 mapper 出口读取过滤结果的辅助
struct FaceHidingEnv {
    MeshData mesh;
    vtkSmartPointer<vtkRenderer> renderer;
    std::shared_ptr<TestMeshActorWithData> actor;

    //! @brief 加载指定网格，未提供输入时使用既有演示网格。
    explicit FaceHidingEnv(MeshData input_mesh = { })
        : mesh(std::move(input_mesh))
    {
        // MeshDataVtk 持有 mesh 成员的引用，mesh 须随本对象保活
        MeshDataVtk model_data = MakeMeshDataVtk(mesh);
        renderer = vtkSmartPointer<vtkRenderer>::New();
        actor = std::make_shared<TestMeshActorWithData>(renderer);
        actor->loadModelData(model_data);
    }

    //! @brief 更新 mapper 的数据生产者并读取已发布的面过滤结果。
    vtkPolyData* faceOutput()
    {
        MeshActorSelectOp select_op(actor);
        auto* face_actor = vtkActor::SafeDownCast(&select_op.getFaceActor());
        auto* mapper = face_actor ? vtkPolyDataMapper::SafeDownCast(face_actor->GetMapper()) : nullptr;
        if (!mapper)
            return nullptr;

        // 渲染端只能更新轻量数据生产者，不能反向执行测试所属线程的数据过滤器。
        auto* connection = mapper->GetInputConnection(0, 0);
        if (auto* producer = connection ? connection->GetProducer() : nullptr)
            producer->Update();
        return mapper->GetInput();
    }
};

//! @brief 读取面管线上透传的 PrecessFaceIds（原模型面 id），并排序便于比对
//!
//! 关键回归点：过滤器必须把该数组按 vtkIdTypeArray 原类型透传（vtkRemovePolyData
//! 会把整型属性提升为 vtkFloatArray，下游拾取 id 的 SafeDownCast 将失效）。
std::vector<vtkIdType> faceIdValues(vtkPolyData* poly)
{
    auto* face_ids = vtkIdTypeArray::SafeDownCast(poly->GetCellData()->GetArray("PrecessFaceIds"));
    REQUIRE(face_ids != nullptr);
    REQUIRE(std::string(face_ids->GetClassName()) == "vtkIdTypeArray");

    std::vector<vtkIdType> values;
    values.reserve(face_ids->GetNumberOfTuples());
    for (vtkIdType i = 0; i < face_ids->GetNumberOfTuples(); ++i)
        values.push_back(face_ids->GetValue(i));

    std::sort(values.begin(), values.end());
    return values;
}

//! @brief 生成自持的属性测试网格，显式准备各类属性以验证有效向量输入。
MeshData makeAttributeMesh()
{
    MeshData mesh = MakeMeshData();
    const auto face_count = mesh.face_vertices_offset_.size() - 1;
    mesh.vertex_attributes_["v_isolation_vector_3"].assign(mesh.vertex_positions_.size() * 3, 1.0);
    mesh.edge_attributes_["e_isolation_vector_3"].assign(mesh.edge_vertices_.size() / 2 * 3, 1.0);
    mesh.face_attributes_["f_isolation_vector_3"].assign(face_count * 3, 1.0);
    mesh.solid_attributes_["s_isolation_vector_3"].assign(mesh.solid_types_.size() * 3, 1.0);
    mesh.face_attributes_["f_isolation_scalar_1"].assign(face_count, 0.5);
    return mesh;
}

} // namespace

TEST_CASE("FaceHideFilter preserves global cell attributes across mixed cell categories")
{
    vtkNew<vtkPolyData> input;
    vtkNew<vtkPoints> points;
    for (vtkIdType id = 0; id < 8; ++id)
        points->InsertNextPoint(static_cast<double>(id % 4), static_cast<double>(id / 4), 0.0);
    input->SetPoints(points);

    // 每类放两个单元，并用不同的属性值识别各分类及分类内的单元顺序。
    vtkNew<vtkCellArray> verts;
    vtkNew<vtkCellArray> lines;
    vtkNew<vtkCellArray> polys;
    vtkNew<vtkCellArray> strips;
    for (vtkIdType id = 0; id < 2; ++id) {
        const vtkIdType cell_points[] = { id, id + 2, id + 4, id + 6 };
        verts->InsertNextCell(1, cell_points);
        lines->InsertNextCell(2, cell_points);
        polys->InsertNextCell(3, cell_points);
        strips->InsertNextCell(4, cell_points);
    }
    input->SetVerts(verts);
    input->SetLines(lines);
    input->SetPolys(polys);
    input->SetStrips(strips);
    vtkNew<vtkIdTypeArray> original_ids;
    original_ids->SetName("OriginalCellIds");
    vtkNew<vtkDoubleArray> values;
    values->SetName("CellValues");
    values->SetNumberOfComponents(2);
    for (vtkIdType id = 0; id < 8; ++id) {
        original_ids->InsertNextValue(id);
        values->InsertNextTuple2(100.0 + static_cast<double>(id), 0.25 + static_cast<double>(id));
    }
    input->GetCellData()->AddArray(original_ids);
    input->GetCellData()->AddArray(values);

    std::vector<vtkIdType> hidden_ids;
    std::vector<vtkIdType> expected_ids;
    SECTION("Each category retains one cell")
    {
        hidden_ids = { 0, 2, 4, 6 };
        expected_ids = { 1, 3, 5, 7 };
    }
    SECTION("Leading categories are entirely removed")
    {
        hidden_ids = { 0, 1, 2, 3 };
        expected_ids = { 4, 5, 6, 7 };
    }
    SECTION("Only vertices remain")
    {
        hidden_ids = { 2, 3, 4, 5, 6, 7 };
        expected_ids = { 0, 1 };
    }
    SECTION("All cells are removed")
    {
        hidden_ids = { 0, 1, 2, 3, 4, 5, 6, 7 };
        expected_ids = { };
    }
    SECTION("Invalid hidden ids still preserve every cell")
    {
        hidden_ids = { -1, 8 };
        expected_ids = { 0, 1, 2, 3, 4, 5, 6, 7 };
    }

    vtkNew<vtkIdTypeArray> hidden;
    for (const auto id : hidden_ids)
        hidden->InsertNextValue(id);
    vtkNew<FaceHideFilter> filter;
    filter->SetInputData(input);
    filter->SetHiddenCellIds(hidden);
    filter->Update();

    // 检查属性类型、元组数量、原始身份及对应单元类型，不能只比较排序后的属性集合。
    const auto check_attributes = [&](const std::vector<vtkIdType>& expected) {
        auto* output = filter->GetOutput();
        const auto count = static_cast<vtkIdType>(expected.size());
        REQUIRE(output->GetNumberOfCells() == count);
        auto* output_ids = vtkIdTypeArray::SafeDownCast(output->GetCellData()->GetArray("OriginalCellIds"));
        auto* output_values = vtkDoubleArray::SafeDownCast(output->GetCellData()->GetArray("CellValues"));
        REQUIRE(output_ids != nullptr);
        REQUIRE(output_values != nullptr);
        REQUIRE(output_ids->GetNumberOfTuples() == count);
        REQUIRE(output_values->GetNumberOfTuples() == count);
        REQUIRE(output_values->GetNumberOfComponents() == 2);
        for (vtkIdType id = 0; id < count; ++id) {
            const auto original_id = expected[static_cast<std::size_t>(id)];
            CAPTURE(id, original_id);
            CHECK(output_ids->GetValue(id) == original_id);
            CHECK(output_values->GetComponent(id, 0) == 100.0 + static_cast<double>(original_id));
            CHECK(output_values->GetComponent(id, 1) == 0.25 + static_cast<double>(original_id));
            CHECK(output->GetCellType(id) == input->GetCellType(original_id));
        }
    };
    check_attributes(expected_ids);

    // 清空隐藏集后恢复所有分类，属性顺序与原始数据完全一致。
    hidden->Reset();
    hidden->Modified();
    filter->Modified();
    filter->Update();
    check_attributes({ 0, 1, 2, 3, 4, 5, 6, 7 });
}

TEST_CASE("MeshPipelineData preserves original typed IDs and published snapshots across hiding and clipping")
{
    MeshData mesh;
    MeshPipelineData data;
    data.loadModelData(MakeMeshDataVtk(mesh));
    const auto original_snapshot = data.createSnapshot();
    REQUIRE(original_snapshot->visible_faces != nullptr);
    REQUIRE(original_snapshot->original_solids != nullptr);
    const auto original_ids = faceIdValues(original_snapshot->visible_faces);
    REQUIRE(original_ids.size() > 2);

    // 体身份与面身份都必须保持 vtkIdTypeArray，避免拾取侧按原类型读取失败。
    auto* solid_ids = vtkIdTypeArray::SafeDownCast(
        original_snapshot->original_solids->GetCellData()->GetArray("vtkOriginalCellIds"));
    REQUIRE(solid_ids != nullptr);
    REQUIRE(solid_ids->GetNumberOfTuples() > 0);
    for (vtkIdType i = 0; i < solid_ids->GetNumberOfTuples(); ++i)
        CHECK(solid_ids->GetValue(i) == i);

    // 模型缓冲可以释放；GUI 源快照必须独立保活已发布坐标、拓扑和属性。
    mesh.clear();
    data.hideFaces({ 0, 2 });
    const auto hidden_snapshot = data.createSnapshot();
    auto expected_ids = original_ids;
    expected_ids.erase(std::remove(expected_ids.begin(), expected_ids.end(), 0), expected_ids.end());
    expected_ids.erase(std::remove(expected_ids.begin(), expected_ids.end(), 2), expected_ids.end());
    REQUIRE(faceIdValues(hidden_snapshot->visible_faces) == expected_ids);
    CHECK(faceIdValues(original_snapshot->visible_faces) == original_ids);

    // 将平面移到全部网格之外，确认新一次裁剪不会重写已发布的隐藏结果。
    vtkNew<vtkPlane> remove_all_plane;
    remove_all_plane->SetOrigin(-1.0e6, 0.0, 0.0);
    remove_all_plane->SetNormal(1.0, 0.0, 0.0);
    data.setClipPlane(remove_all_plane);
    const auto clipped_snapshot = data.createSnapshot();
    REQUIRE(clipped_snapshot->visible_faces->GetNumberOfCells() == 0);
    CHECK(faceIdValues(hidden_snapshot->visible_faces) == expected_ids);
    CHECK(faceIdValues(original_snapshot->visible_faces) == original_ids);

    data.setClipPlane(nullptr);
    data.showAllFaces();
    const auto restored_snapshot = data.createSnapshot();
    CHECK(faceIdValues(restored_snapshot->visible_faces) == original_ids);
    CHECK(faceIdValues(hidden_snapshot->visible_faces) == expected_ids);
    CHECK(clipped_snapshot->visible_faces->GetNumberOfCells() == 0);
}

TEST_CASE("MeshActor consumes completed mesh data through trivial producers")
{
    MeshData mesh;
    MeshPipelineData data;
    data.loadModelData(MakeMeshDataVtk(mesh));
    data.hideFaces({ 0, 2 });
    vtkNew<vtkPlane> keep_all_plane;
    keep_all_plane->SetOrigin(1.0e6, 0.0, 0.0);
    keep_all_plane->SetNormal(1.0, 0.0, 0.0);
    data.setClipPlane(keep_all_plane);
    const auto snapshot = data.createSnapshot();
    const auto published_ids = faceIdValues(snapshot->visible_faces);

    vtkNew<vtkRenderer> renderer;
    auto actor = std::make_shared<MeshActor>(renderer);
    actor->applyDataSnapshot(snapshot);
    MeshActorSelectOp select_op(actor);

    // 三类 mapper 均只接收数据；渲染更新不能沿连接触发 GUI 计算管线。
    for (vtkProp* prop : { &select_op.getFaceActor(), &select_op.getEdgeActor(), &select_op.getSolidActor() }) {
        auto* mesh_actor = vtkActor::SafeDownCast(prop);
        REQUIRE(mesh_actor != nullptr);
        auto* mapper = vtkPolyDataMapper::SafeDownCast(mesh_actor->GetMapper());
        REQUIRE(mapper != nullptr);
        auto* connection = mapper->GetInputConnection(0, 0);
        REQUIRE(connection != nullptr);
        CHECK(vtkTrivialProducer::SafeDownCast(connection->GetProducer()) != nullptr);
    }

    // 数据侧继续更新后，未安装新快照的 mapper 必须仍展示上一次发布的面集。
    data.hideFaces({ 1 });
    const auto newer_snapshot = data.createSnapshot();
    REQUIRE(faceIdValues(newer_snapshot->visible_faces) != published_ids);
    auto* face_actor = vtkActor::SafeDownCast(&select_op.getFaceActor());
    auto* face_mapper = vtkPolyDataMapper::SafeDownCast(face_actor->GetMapper());
    face_mapper->Update();
    CHECK(faceIdValues(face_mapper->GetInput()) == published_ids);
    actor->applyDataSnapshot(newer_snapshot);
    actor->applyDataSnapshot(snapshot); // 旧版本不能覆盖已经安装的新结果。
    face_mapper->Update();
    CHECK(faceIdValues(face_mapper->GetInput()) == faceIdValues(newer_snapshot->visible_faces));
}

TEST_CASE("MeshActorManager preserves scalar bar across same-generation snapshots and clears it on reload")
{
    MeshData mesh = makeAttributeMesh();
    MeshPipelineData data;
    data.loadModelData(MakeMeshDataVtk(mesh));
    const auto initial_snapshot = data.createSnapshot();
    vtkNew<vtkRenderer> renderer;
    MeshActorManager manager;
    manager.bindRender(renderer);
    manager.loadMesh(7, initial_snapshot, renderer);
    manager.setAttriMode(7, "f_isolation_scalar_1", Mode::SCALAR,
        { { "scalar_range", std::vector<double> { 0.0, 1.0 } } });

    // 从渲染器查找实际颜色条，避免测试私有成员或仅验证属性调用成功。
    vtkScalarBarActor* scalar_bar = nullptr;
    auto* props = renderer->GetViewProps();
    props->InitTraversal();
    while (auto* prop = props->GetNextProp()) {
        if (auto* candidate = vtkScalarBarActor::SafeDownCast(prop)) {
            REQUIRE(scalar_bar == nullptr);
            scalar_bar = candidate;
        }
    }
    REQUIRE(scalar_bar != nullptr);
    REQUIRE(scalar_bar->GetVisibility());
    REQUIRE(scalar_bar->GetLookupTable() != nullptr);
    REQUIRE(std::string(scalar_bar->GetTitle()) == "isolation_scalar");
    auto* lookup_table = scalar_bar->GetLookupTable();
    AttributeOperator attribute_op(manager.getComponentActor(7).get());
    REQUIRE(attribute_op.getFaceMapper()->GetScalarVisibility());

    SECTION("manual face hiding") { data.hideFaces({ 0 }); }
    SECTION("clipping")
    {
        vtkNew<vtkPlane> plane;
        plane->SetOrigin(1.0e6, 0.0, 0.0);
        plane->SetNormal(1.0, 0.0, 0.0);
        data.setClipPlane(plane);
    }
    SECTION("overall visibility") { data.setVisibility(false); }

    // 同一份网格的显示更新必须保留属性模式、颜色条及其颜色映射。
    const auto updated_snapshot = data.createSnapshot();
    REQUIRE(updated_snapshot->generation == initial_snapshot->generation);
    manager.loadMesh(7, updated_snapshot, renderer);
    CHECK(scalar_bar->GetVisibility());
    CHECK(scalar_bar->GetLookupTable() == lookup_table);
    CHECK(std::string(scalar_bar->GetTitle()) == "isolation_scalar");
    CHECK(attribute_op.getFaceMapper()->GetScalarVisibility());

    // 重新加载会生成新的网格代次，旧属性模式及颜色条仍须按原契约清理。
    data.loadModelData(MakeMeshDataVtk(mesh));
    const auto reloaded_snapshot = data.createSnapshot();
    REQUIRE(reloaded_snapshot->generation != initial_snapshot->generation);
    manager.loadMesh(7, reloaded_snapshot, renderer);
    CHECK_FALSE(scalar_bar->GetVisibility());
    CHECK_FALSE(attribute_op.getFaceMapper()->GetScalarVisibility());

    // 新网格重新着色后，旧代次快照不能关闭颜色条或替换当前数据。
    manager.setAttriMode(7, "f_isolation_scalar_1", Mode::SCALAR, {});
    REQUIRE(scalar_bar->GetVisibility());
    manager.loadMesh(7, initial_snapshot, renderer);
    CHECK(scalar_bar->GetVisibility());
    CHECK(manager.getComponentActor(7)->dataGeneration() == reloaded_snapshot->generation);
}

TEST_CASE("hideFaces ignores duplicate and invalid IDs and showAllFaces restores them")
{
    FaceHidingEnv env;

    vtkPolyData* output = env.faceOutput();
    const vtkIdType original_count = output->GetNumberOfCells();
    REQUIRE(original_count > 4);

    env.actor->hideFaces({ 0 });
    env.actor->hideFaces({ 0, 2, original_count + 100, -1 });
    output = env.faceOutput();
    REQUIRE(output->GetNumberOfCells() == original_count - 2);

    std::vector<vtkIdType> expected;
    for (vtkIdType id = 0; id < original_count; ++id) {
        if (id != 0 && id != 2)
            expected.push_back(id);
    }
    REQUIRE(faceIdValues(output) == expected);

    env.actor->showAllFaces();
    output = env.faceOutput();
    REQUIRE(output->GetNumberOfCells() == original_count);

    expected.clear();
    for (vtkIdType id = 0; id < original_count; ++id)
        expected.push_back(id);
    REQUIRE(faceIdValues(output) == expected);
}

TEST_CASE("reverseVisibility complements hidden faces and preserves clipping")
{
    FaceHidingEnv env;
    const auto original_ids = faceIdValues(env.faceOutput());
    REQUIRE(original_ids.size() > 2);
    vtkNew<vtkPlane> keep_all_plane;
    keep_all_plane->SetOrigin(1.0e6, 0.0, 0.0);
    keep_all_plane->SetNormal(1.0, 0.0, 0.0);
    env.actor->setClipPlane(keep_all_plane);
    env.actor->hideFaces({ 0, 2 });
    const auto partially_visible = faceIdValues(env.faceOutput());

    REQUIRE(env.actor->reverseVisibility());
    CHECK(faceIdValues(env.faceOutput()) == std::vector<vtkIdType>({ 0, 2 }));
    REQUIRE(env.actor->reverseVisibility());
    CHECK(faceIdValues(env.faceOutput()) == partially_visible);
    env.actor->setClipPlane(nullptr);
    CHECK(faceIdValues(env.faceOutput()) == partially_visible);

    // 整体隐藏必须按实际不可见状态反转，不能只恢复之前剩余的面。
    env.actor->setVisibility(false);
    REQUIRE(env.actor->reverseVisibility());
    CHECK(env.actor->isVisible());
    CHECK(faceIdValues(env.faceOutput()) == original_ids);
    CHECK_FALSE(env.actor->reverseVisibility());
    CHECK_FALSE(env.actor->isVisible());
    REQUIRE(env.actor->reverseVisibility());
    CHECK(faceIdValues(env.faceOutput()) == original_ids);

    env.actor->hideFaces(original_ids);
    CHECK(env.faceOutput()->GetNumberOfCells() == 0);
    REQUIRE(env.actor->reverseVisibility());
    CHECK(faceIdValues(env.faceOutput()) == original_ids);
}

TEST_CASE("showFaces restores requested faces while preserving other hidden faces")
{
    FaceHidingEnv env;
    const auto original_ids = faceIdValues(env.faceOutput());
    env.actor->hideFaces({ 0, 1, 2 });
    env.actor->showFaces({ 0, 0, -1, static_cast<vtkIdType>(original_ids.size()) });
    auto expected = original_ids;
    expected.erase(std::remove(expected.begin(), expected.end(), 1), expected.end());
    expected.erase(std::remove(expected.begin(), expected.end(), 2), expected.end());
    CHECK(faceIdValues(env.faceOutput()) == expected);
    env.actor->showFaces({ });
    CHECK(faceIdValues(env.faceOutput()) == expected);

    // 父组件隐藏时仅恢复指定面集合，其他渲染对象继续遵循当前组件开关与样式。
    env.actor->setVisibility(false);
    env.actor->showFaces({ -1 });
    CHECK_FALSE(env.actor->isVisible());
    env.actor->showFaces({ 1 });
    CHECK(env.actor->isVisible());
    CHECK(faceIdValues(env.faceOutput()) == std::vector<vtkIdType>({ 1 }));
    MeshActorSelectOp op(env.actor);
    CHECK(op.getSolidActor().GetVisibility());
    CHECK(op.getEdgeActor().GetVisibility());
}

TEST_CASE("face isolation survives style changes and ends on restore or reload")
{
    FaceHidingEnv env;
    const auto original_ids = faceIdValues(env.faceOutput());
    MeshActorSelectOp op(env.actor);
    env.actor->isolateFaces({ 0, 2 });
    CHECK(faceIdValues(env.faceOutput()) == std::vector<vtkIdType>({ 0, 2 }));
    env.actor->setRenderStyle(MeshRenderStyle::Face);
    env.actor->setRenderStyle(MeshRenderStyle::FaceWithEdges);
    CHECK(op.getSolidActor().GetVisibility());
    CHECK(op.getEdgeActor().GetVisibility());
    env.actor->showFaces({ 1 });
    CHECK(faceIdValues(env.faceOutput()) == std::vector<vtkIdType>({ 0, 1, 2 }));
    CHECK(op.getSolidActor().GetVisibility());
    env.actor->showAllFaces();
    CHECK(faceIdValues(env.faceOutput()) == original_ids);
    CHECK(op.getSolidActor().GetVisibility());
    CHECK(op.getEdgeActor().GetVisibility());

    // 隔离全部面没有隐藏 id，也必须能结束隔离。
    env.actor->isolateFaces(original_ids);
    env.actor->showAllFaces();
    CHECK(op.getSolidActor().GetVisibility());
    env.actor->isolateFaces({ 0 });
    MeshData reloaded_mesh;
    env.actor->loadModelData(MakeMeshDataVtk(reloaded_mesh));
    CHECK(faceIdValues(env.faceOutput()) == original_ids);
    CHECK(op.getSolidActor().GetVisibility());
}

//! @brief 面隔离只改变面集合，向量和标量显示仍由原有属性策略决定。
TEST_CASE("face isolation preserves ordinary vector and scalar attribute rendering")
{
    FaceHidingEnv env(makeAttributeMesh());
    AttributeOperator attribute_op(env.actor.get());
    MeshActorSelectOp select_op(env.actor);
    auto* glyph = attribute_op.getGlyph3DActor();
    std::string attribute;
    SECTION("vertex vectors") { attribute = "v_isolation_vector_3"; }
    SECTION("edge vectors") { attribute = "e_isolation_vector_3"; }
    SECTION("face vectors") { attribute = "f_isolation_vector_3"; }
    SECTION("solid vectors") { attribute = "s_isolation_vector_3"; }

    // 先确认向量属性有效且生成了箭头，避免空输入使隔离断言假通过。
    env.actor->setRenderStrategy(std::make_unique<AttriRenderStrategyVector>());
    env.actor->renderAttribute(attribute, { });
    attribute_op.getGlyph3DMapper()->Update();
    REQUIRE(attribute_op.getGlyph3DMapper()->GetInput() != nullptr);
    REQUIRE(attribute_op.getGlyph3DMapper()->GetInput()->GetNumberOfCells() > 0);
    REQUIRE(glyph->GetVisibility());
    env.actor->isolateFaces({ 0, 2 });
    REQUIRE(glyph->GetVisibility());

    // 隔离不再屏蔽同组件的向量、体和独立边。
    env.actor->renderAttribute(attribute, { });
    CHECK(glyph->GetVisibility());
    CHECK(select_op.getSolidActor().GetVisibility());
    CHECK(select_op.getEdgeActor().GetVisibility());
    CHECK(faceIdValues(env.faceOutput()) == std::vector<vtkIdType>({ 0, 2 }));
    env.actor->setRenderStrategy(std::make_unique<AttriRenderStrategyScalar>());
    env.actor->renderAttribute("f_isolation_scalar_1", { });
    CHECK_FALSE(glyph->GetVisibility());
    CHECK(select_op.getSolidActor().GetVisibility());
    CHECK(faceIdValues(env.faceOutput()) == std::vector<vtkIdType>({ 0, 2 }));

    // 同一场景结束隔离后验证策略恢复，避免另建重复的属性测试环境。
    env.actor->showAllFaces();
    env.actor->setRenderStrategy(std::make_unique<AttriRenderStrategyVector>());
    env.actor->renderAttribute(attribute, { });
    CHECK(glyph->GetVisibility());
    env.actor->setRenderStrategy(std::make_unique<AttriRenderStrategyScalar>());
    env.actor->renderAttribute("f_isolation_scalar_1", { });
    CHECK_FALSE(glyph->GetVisibility());
    CHECK(attribute_op.getFaceMapper()->GetScalarVisibility());
}

namespace {

//! @brief 生成规则三角面网格，覆盖原始 32 位连通性与隐藏后压缩连通性的切换。
MeshData makeTriangularSurface()
{
    MeshData mesh;
    constexpr Index side = 32;
    for (Index y = 0; y <= side; ++y) {
        for (Index x = 0; x <= side; ++x)
            mesh.vertex_positions_.push_back({ static_cast<double>(x), static_cast<double>(y), 0.0 });
    }
    mesh.vertex_count_ = static_cast<Index>(mesh.vertex_positions_.size());
    mesh.face_vertices_offset_.push_back(0);
    // 每个方格拆成两个三角面；面内部的黑色边线用于检查实际渲染结果。
    for (Index y = 0; y < side; ++y) {
        for (Index x = 0; x < side; ++x) {
            const Index a = y * (side + 1) + x;
            const Index b = a + 1;
            const Index c = a + side + 1;
            const Index d = c + 1;
            mesh.face_vertices_.insert(mesh.face_vertices_.end(), { a, b, d, a, d, c });
            mesh.face_vertices_offset_.push_back(static_cast<Index>(mesh.face_vertices_.size()) - 3);
            mesh.face_vertices_offset_.push_back(static_cast<Index>(mesh.face_vertices_.size()));
        }
    }
    return mesh;
}

//! @brief 读取刚完成的一帧，统计模型内部黑色线框像素，排除背景和外轮廓。
std::size_t countInteriorEdgePixels(vtkRenderWindow& window)
{
    window.Render();
    vtkNew<vtkWindowToImageFilter> capture;
    capture->SetInput(&window);
    capture->ReadFrontBufferOff();
    capture->ShouldRerenderOff();
    capture->Update();
    auto* image = capture->GetOutput();
    std::size_t count = 0;
    for (int y = 64; y < 256; ++y) {
        for (int x = 64; x < 256; ++x) {
            auto* pixel = static_cast<unsigned char*>(image->GetScalarPointer(x, y, 0));
            if (pixel[0] < 80 && pixel[1] < 80 && pixel[2] < 80)
                ++count;
        }
    }
    return count;
}

} // namespace

TEST_CASE("showAllFaces restores triangular surface wireframe after extensive hiding")
{
    FaceHidingEnv env(makeTriangularSurface());
    vtkNew<vtkRenderWindow> window;
    window->SetOffScreenRendering(1);
    window->SetMultiSamples(0);
    window->SetSize(320, 320);
    window->AddRenderer(env.renderer);
    env.actor->setRenderStyle(MeshRenderStyle::FaceWithEdges);
    MeshActorSelectOp select_op(env.actor);
    auto* face_actor = vtkActor::SafeDownCast(&select_op.getFaceActor());
    REQUIRE(face_actor != nullptr);
    face_actor->GetProperty()->SetColor(1.0, 1.0, 1.0);
    face_actor->GetProperty()->SetEdgeColor(0.0, 0.0, 0.0);
    face_actor->GetProperty()->LightingOff();
    env.renderer->SetBackground(0.7, 0.7, 0.7);
    auto* camera = env.renderer->GetActiveCamera();
    camera->SetPosition(16.0, 16.0, 100.0);
    camera->SetFocalPoint(16.0, 16.0, 0.0);
    camera->SetViewUp(0.0, 1.0, 0.0);
    camera->ParallelProjectionOn();
    camera->SetParallelScale(20.0);
    env.renderer->ResetCameraClippingRange();
    const auto original_edges = countInteriorEdgePixels(*window);
    REQUIRE(original_edges > 1000);
    const auto original_count = env.faceOutput()->GetNumberOfCells();

    // 连续隐藏中间约九成面、渲染压缩网格、再全部恢复，不能仅检查 EdgeVisibility 开关。
    std::vector<vtkIdType> hidden_ids;
    for (vtkIdType id = 64; id < original_count - 64; ++id)
        hidden_ids.push_back(id);
    for (int round = 0; round < 3; ++round) {
        env.actor->hideFaces(hidden_ids);
        REQUIRE(env.faceOutput()->GetNumberOfCells() == 128);
        countInteriorEdgePixels(*window);
        // 真实硬件拾取会切换选择着色器，随后一帧在压缩网格上恢复普通绘制着色器。
        vtkNew<vtkHardwareSelector> selector;
        selector->SetRenderer(env.renderer);
        selector->SetFieldAssociation(vtkDataObject::FIELD_ASSOCIATION_CELLS);
        selector->SetArea(0, 0, 319, 319);
        vtkSmartPointer<vtkSelection> selection;
        selection.TakeReference(selector->Select());
        REQUIRE(selection != nullptr);
        countInteriorEdgePixels(*window);
        // 反转也会切换面连通性，必须保留实际线框绘制能力。
        REQUIRE(env.actor->reverseVisibility());
        REQUIRE(env.faceOutput()->GetNumberOfCells() == original_count - 128);
        REQUIRE(countInteriorEdgePixels(*window) > 1000);
        REQUIRE(env.actor->reverseVisibility());
        REQUIRE(env.faceOutput()->GetNumberOfCells() == 128);
        env.actor->isolateFaces(hidden_ids);
        REQUIRE(env.faceOutput()->GetNumberOfCells() == original_count - 128);
        REQUIRE(countInteriorEdgePixels(*window) > 1000);
        env.actor->showAllFaces();
        REQUIRE(env.faceOutput()->GetNumberOfCells() == original_count);
        REQUIRE(face_actor->GetProperty()->GetEdgeVisibility() != 0);
        CAPTURE(round, original_edges);
        REQUIRE(countInteriorEdgePixels(*window) == original_edges);
    }
}
