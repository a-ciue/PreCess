#include "MakeMeshDataVtk.h"

#include "MeshActor.h"
#include "MeshActorSelectOp.h"
#include <catch2/catch_test_macros.hpp>
#include <vtkActor.h>
#include <vtkAlgorithmOutput.h>
#include <vtkCamera.h>
#include <vtkCellData.h>
#include <vtkHardwareSelector.h>
#include <vtkIdTypeArray.h>
#include <vtkImageData.h>
#include <vtkNew.h>
#include <vtkPlane.h>
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

} // namespace

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
        env.actor->showAllFaces();
        REQUIRE(env.faceOutput()->GetNumberOfCells() == original_count);
        REQUIRE(face_actor->GetProperty()->GetEdgeVisibility() != 0);
        CAPTURE(round, original_edges);
        REQUIRE(countInteriorEdgePixels(*window) == original_edges);
    }
}
