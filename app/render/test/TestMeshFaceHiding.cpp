#include "MakeMeshDataVtk.h"
#include "MakeMeshData.h"

#include "MeshActor.h"
#include "MeshActorSelectOp.h"
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
#include <vtkRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkSelection.h>
#include <vtkSmartPointer.h>
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
    std::shared_ptr<MeshActor> actor;

    //! @brief 加载指定网格，未提供输入时使用既有演示网格。
    explicit FaceHidingEnv(MeshData input_mesh = { })
        : mesh(std::move(input_mesh))
    {
        // MeshDataVtk 持有 mesh 成员的引用，mesh 须随本对象保活
        MeshDataVtk model_data = MakeMeshDataVtk(mesh);
        renderer = vtkSmartPointer<vtkRenderer>::New();
        actor = std::make_shared<MeshActor>(renderer);
        actor->loadModelData(model_data);
    }

    //! @brief 执行面管线并返回 face mapper 当前输入（隐藏/裁剪过滤器的输出）
    vtkPolyData* faceOutput()
    {
        MeshActorSelectOp select_op(actor);
        auto* face_actor = vtkActor::SafeDownCast(&select_op.getFaceActor());
        auto* mapper = face_actor ? vtkPolyDataMapper::SafeDownCast(face_actor->GetMapper()) : nullptr;
        if (!mapper)
            return nullptr;

        // 经输入连接的生产者算法驱动管线执行（SetInputData 时为 trivial producer，Update 无害）。
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

TEST_CASE("hideFaces removes selected cells and showAllFaces restores them")
{
    FaceHidingEnv env;

    vtkPolyData* output = env.faceOutput();
    const vtkIdType original_count = output->GetNumberOfCells();
    REQUIRE(original_count > 4);

    env.actor->hideFaces({ 0, 2 });
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

TEST_CASE("hideFaces accumulates, deduplicates and ignores out-of-range ids")
{
    FaceHidingEnv env;
    const vtkIdType original_count = env.faceOutput()->GetNumberOfCells();

    env.actor->hideFaces({ 0 });
    env.actor->hideFaces({ 0, 1, original_count + 100, -1 });
    vtkPolyData* output = env.faceOutput();

    REQUIRE(output->GetNumberOfCells() == original_count - 2);

    std::vector<vtkIdType> expected;
    for (vtkIdType id = 0; id < original_count; ++id) {
        if (id != 0 && id != 1)
            expected.push_back(id);
    }
    REQUIRE(faceIdValues(output) == expected);
}

TEST_CASE("face hiding composes with the clipping pipeline and survives clip toggling")
{
    FaceHidingEnv env;
    const vtkIdType original_count = env.faceOutput()->GetNumberOfCells();

    // 原点远离网格、法向指向 +X 的平面：原点附近所有点的隐函数值严格为负（内侧），
    // 演示网格全部保留，仅用于验证 hider → clipper 的串联接线，不实际剔除单元。
    vtkNew<vtkPlane> keep_all_plane;
    keep_all_plane->SetOrigin(1.0e6, 0.0, 0.0);
    keep_all_plane->SetNormal(1.0, 0.0, 0.0);

    env.actor->setClipPlane(keep_all_plane);
    REQUIRE(env.faceOutput()->GetNumberOfCells() == original_count);

    // 裁剪开启时隐藏：隐藏过滤器位于裁剪上游，两个 id 仍被移除。
    env.actor->hideFaces({ 0, 1 });
    REQUIRE(env.faceOutput()->GetNumberOfCells() == original_count - 2);

    // 关闭裁剪不影响隐藏状态（接线在 hider 直连 mapper 与 hider → clipper 间切换）。
    env.actor->setClipPlane(nullptr);
    REQUIRE(env.faceOutput()->GetNumberOfCells() == original_count - 2);

    env.actor->showAllFaces();
    REQUIRE(env.faceOutput()->GetNumberOfCells() == original_count);
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

    // 父组件隐藏时，显示所选面不会恢复同组件其余面、体或独立边。
    env.actor->setVisibility(false);
    env.actor->showFaces({ -1 });
    CHECK_FALSE(env.actor->isVisible());
    env.actor->showFaces({ 1 });
    CHECK(env.actor->isVisible());
    CHECK(faceIdValues(env.faceOutput()) == std::vector<vtkIdType>({ 1 }));
    MeshActorSelectOp op(env.actor);
    CHECK_FALSE(op.getSolidActor().GetVisibility());
    CHECK_FALSE(op.getEdgeActor().GetVisibility());
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
    CHECK_FALSE(op.getSolidActor().GetVisibility());
    CHECK_FALSE(op.getEdgeActor().GetVisibility());
    env.actor->showFaces({ 1 });
    CHECK(faceIdValues(env.faceOutput()) == std::vector<vtkIdType>({ 0, 1, 2 }));
    CHECK_FALSE(op.getSolidActor().GetVisibility());
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

//! @brief 隔离约束必须覆盖所有向量属性来源及后续标量渲染。
TEST_CASE("face isolation constrains vector and scalar attribute rendering")
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
    REQUIRE_FALSE(glyph->GetVisibility());

    // 隔离后重新应用向量，属性策略不能打开不经过面过滤管线的覆盖层。
    env.actor->renderAttribute(attribute, { });
    CHECK_FALSE(glyph->GetVisibility());
    CHECK_FALSE(select_op.getSolidActor().GetVisibility());
    CHECK_FALSE(select_op.getEdgeActor().GetVisibility());
    CHECK(faceIdValues(env.faceOutput()) == std::vector<vtkIdType>({ 0, 2 }));
    env.actor->setRenderStrategy(std::make_unique<AttriRenderStrategyScalar>());
    env.actor->renderAttribute("f_isolation_scalar_1", { });
    CHECK_FALSE(glyph->GetVisibility());
    CHECK_FALSE(select_op.getSolidActor().GetVisibility());
    CHECK(faceIdValues(env.faceOutput()) == std::vector<vtkIdType>({ 0, 2 }));
}

//! @brief 结束隔离后保留普通向量显示和切换标量时的箭头取消行为。
TEST_CASE("attribute rendering after face isolation restores ordinary vector and scalar behavior")
{
    FaceHidingEnv env(makeAttributeMesh());
    AttributeOperator attribute_op(env.actor.get());
    env.actor->isolateFaces({ 0 });
    env.actor->showAllFaces();
    // 结束隔离后向量应正常显示，切换标量则继续遵守策略自身的取消逻辑。
    env.actor->setRenderStrategy(std::make_unique<AttriRenderStrategyVector>());
    env.actor->renderAttribute("f_isolation_vector_3", { });
    REQUIRE(attribute_op.getGlyph3DActor()->GetVisibility());
    env.actor->setRenderStrategy(std::make_unique<AttriRenderStrategyScalar>());
    env.actor->renderAttribute("f_isolation_scalar_1", { });
    CHECK_FALSE(attribute_op.getGlyph3DActor()->GetVisibility());
    CHECK(attribute_op.getFaceMapper()->GetScalarVisibility());
}

TEST_CASE("reloading mesh data clears hidden faces")
{
    FaceHidingEnv env;
    const vtkIdType original_count = env.faceOutput()->GetNumberOfCells();

    env.actor->hideFaces({ 0, 1, 2 });
    REQUIRE(env.faceOutput()->GetNumberOfCells() == original_count - 3);

    // 重载同构网格：旧隐藏 id 语义失效，管线恢复完整面集。
    MeshData reloaded_mesh;
    MeshDataVtk reloaded_data = MakeMeshDataVtk(reloaded_mesh);
    env.actor->loadModelData(reloaded_data);

    REQUIRE(env.faceOutput()->GetNumberOfCells() == original_count);
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
