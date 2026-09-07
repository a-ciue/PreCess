#include "MakeMeshDataVtk.h"

#include "MeshActor.h"
#include "MeshActorSelectOp.h"
#include "SelectorHighlight.h"
#include <catch2/catch_test_macros.hpp>
#include <vtkNew.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPolyData.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkSmartPointer.h>

#include <array>
#include <memory>
#include <utility>

namespace {

//! @brief 离屏渲染环境：加载演示网格并保持 MeshActor 存活，供选择器拾取
struct OffscreenMesh {
    MeshData mesh;
    vtkSmartPointer<vtkRenderer> renderer;
    vtkSmartPointer<vtkRenderWindow> render_window;
    std::shared_ptr<MeshActor> actor;

    OffscreenMesh()
    {
        // MeshDataVtk 持有 mesh 成员的引用，mesh 须随本对象保活
        MeshDataVtk model_data = MakeMeshDataVtk(mesh);
        renderer = vtkSmartPointer<vtkRenderer>::New();
        render_window = vtkSmartPointer<vtkRenderWindow>::New();
        render_window->SetOffScreenRendering(1);
        render_window->SetMultiSamples(0);
        render_window->AddRenderer(renderer);
        render_window->SetSize(600, 600);
        actor = std::make_shared<MeshActor>(renderer);
        actor->loadModelData(model_data);
        renderer->ResetCamera();
        render_window->Render();
    }

    //! @brief 世界坐标投到屏幕坐标（display 坐标，与 picker::Pick 同一约定）
    std::pair<double, double> projectToDisplay(const std::array<double, 3>& world) const
    {
        renderer->SetWorldPoint(world[0], world[1], world[2], 1.0);
        renderer->WorldToDisplay();
        double display[3] {};
        renderer->GetDisplayPoint(display);
        return { display[0], display[1] };
    }
};

//! @brief partition 上的高亮网格 cell 数（约定空高亮 = partition 为空数据或已摘除）
std::size_t partitionCellCount(vtkPartitionedDataSet& highlight_data)
{
    auto* poly = vtkPolyData::SafeDownCast(highlight_data.GetPartition(0));
    return poly ? static_cast<std::size_t>(poly->GetNumberOfCells()) : 0;
}

} // namespace

TEST_CASE("FaceSelectorHighlight clears highlight when last face is deselected")
{
    OffscreenMesh env;

    vtkNew<vtkPartitionedDataSet> highlight_data;
    FaceSelectorHighlight selector(*env.renderer, *highlight_data, 0, MeshActorSelectOp(env.actor));

    // 拾取目标：独立四边形（不与体表面重叠），其中心
    const auto [x, y] = env.projectToDisplay({ 2.0, 0.5, 0.0 });
    selector.select(x, y);
    REQUIRE(partitionCellCount(*highlight_data) == 1);

    // 再次点击同一位置取消选中：高亮必须清空而不是残留上一次的内容
    selector.select(x, y);
    REQUIRE(partitionCellCount(*highlight_data) == 0);
}

TEST_CASE("EdgeSelectorHighlight clears highlight when last edge is deselected")
{
    OffscreenMesh env;

    vtkNew<vtkPartitionedDataSet> highlight_data;
    EdgeSelectorHighlight selector(*env.renderer, *highlight_data, 0, MeshActorSelectOp(env.actor), 0, nullptr);

    // 拾取目标：立方体顶边 (4,5) 的中点
    const auto [x, y] = env.projectToDisplay({ 0.5, 0.0, 1.0 });
    selector.select(x, y);
    REQUIRE(partitionCellCount(*highlight_data) == 1);

    selector.select(x, y);
    REQUIRE(partitionCellCount(*highlight_data) == 0);
}

TEST_CASE("VertexSelectorHighlight clears highlight when last vertex is deselected")
{
    OffscreenMesh env;

    vtkNew<vtkPartitionedDataSet> highlight_data;
    VertexSelectorHighlight selector(*env.renderer, *highlight_data, 0, MeshActorSelectOp(env.actor), 0, nullptr);

    // 拾取目标：楔体三角形面（面1）的顶点 14。注意 POINT pass 对四边形 cell 失效
    // （实验确认 quad cell 拾取正常而点拾取为空），必须落在三角形网格上。
    const auto [x, y] = env.projectToDisplay({ 3.0, 0.0, 0.0 });

    selector.select(x, y);
    REQUIRE(partitionCellCount(*highlight_data) == 1);

    selector.select(x, y);
    // 顶点/体路径空选择时摘掉 partition（vtkGeometryFilter 对空输入不清输出，不能走过滤器）
    REQUIRE(highlight_data->GetPartition(0) == nullptr);
    REQUIRE(partitionCellCount(*highlight_data) == 0);
}

TEST_CASE("SolidSelectorHighlight clears highlight when last solid is deselected")
{
    OffscreenMesh env;

    vtkNew<vtkPartitionedDataSet> highlight_data;
    SolidSelectorHighlight selector(*env.renderer, *highlight_data, 0, MeshActorSelectOp(env.actor));

    // 拾取目标：六面体中心
    const auto [x, y] = env.projectToDisplay({ 0.5, 0.5, 0.5 });
    selector.select(x, y);
    REQUIRE(partitionCellCount(*highlight_data) > 0);

    selector.select(x, y);
    REQUIRE(highlight_data->GetPartition(0) == nullptr);
    REQUIRE(partitionCellCount(*highlight_data) == 0);
}
