#include "MakeMeshDataVtk.h"
#include "MeshIdQuery.h"
#include "SelectorHighlight.h"
#include <iostream>
#include <string>
#include <vtkPartitionedDataSet.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkNew.h>
#include <vtkActor.h>
#include <vtkDataSet.h>
#include <vtkExtractSelection.h>
#include <vtkIdTypeArray.h>
#include <vtkPointData.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkPoints.h>
#include <vtkUnstructuredGrid.h>

namespace {
// 提取结果中是否包含指定局部点 id
// （vtkOriginalPointIds：提取链上为 VTK 自动生成的输入下标数组，与本组件局部点 id 同语义）
bool outputHasLocalId(vtkExtractSelection* extract, Index local_id)
{
    auto* output = vtkDataSet::SafeDownCast(extract->GetOutputDataObject(0));
    if (!output)
        return false;
    auto* ids = vtkIdTypeArray::SafeDownCast(
        output->GetPointData()->GetArray("vtkOriginalPointIds"));
    if (!ids)
        return false;
    for (vtkIdType i = 0; i < ids->GetNumberOfTuples(); ++i) {
        if (ids->GetValue(i) == local_id)
            return true;
    }
    return false;
}

// 非交互自检：顶点高亮回归——extractVertex 活引用选择列表随更新生效
int selfCheck()
{
    MeshData mesh;
    MeshDataVtk test_mesh_data = MakeMeshDataVtk(mesh);

    vtkNew<vtkRenderer> renderer;
    auto mesh_actor = std::make_shared<MeshActor>(renderer);
    mesh_actor->loadModelData(test_mesh_data);
    MeshActorSelectOp select_op(mesh_actor);

    int failures = 0;
    auto check = [&](bool ok, const char* msg) {
        if (!ok) {
            ++failures;
            std::cerr << "FAIL: " << msg << std::endl;
        }
    };

    // 活引用选择列表：构造时为空，更新数组后管线重取生效（本次回归点）
    vtkNew<vtkIdTypeArray> live_ids;
    auto extract = select_op.extractVertex(live_ids);
    extract->Update();
    check(!outputHasLocalId(extract, 0) && !outputHasLocalId(extract, 3), "初始空提取");

    live_ids->InsertNextValue(0);
    live_ids->InsertNextValue(3);
    live_ids->Modified();
    extract->Update();
    check(outputHasLocalId(extract, 0) && outputHasLocalId(extract, 3), "插入局部 id 后提取出对应点");

    live_ids->SetNumberOfValues(0);
    live_ids->Modified();
    extract->Update();
    check(!outputHasLocalId(extract, 0) && !outputHasLocalId(extract, 3), "清空后提取为空");

    if (failures == 0)
        std::cout << "VertexSelectorHighlight self-check passed" << std::endl;
    return failures == 0 ? 0 : 1;
}

//! @brief id 查询桩：交互演示组件单一，pointGlobalId 直通局部 id（iota 恒等）
class StubMeshIdQuery : public IMeshIdQuery {
public:
    std::optional<Index> findEdgeByEndpoints(Index, Index, Index) const override { return std::nullopt; }
    Index pointGlobalId(Index, Index local_point_id) const override { return local_point_id; }
};
} // namespace

// 自定义交互器，响应鼠标左键点击
class VertexPickInteractorStyle : public vtkInteractorStyleTrackballCamera {
public:
    static VertexPickInteractorStyle* New();
    vtkTypeMacro(VertexPickInteractorStyle, vtkInteractorStyleTrackballCamera);

    void SetSelectorHighlight(VertexSelectorHighlight* selector)
    {
        this->Selector = selector;
    }
    void OnLeftButtonDown() override
    {
        int* clickPos = this->GetInteractor()->GetEventPosition();
        if (Selector) {
            Selector->select(static_cast<double>(clickPos[0]), static_cast<double>(clickPos[1]));
        }
        vtkInteractorStyleTrackballCamera::OnLeftButtonDown();
    }

private:
    VertexSelectorHighlight* Selector = nullptr;
};

vtkStandardNewMacro(VertexPickInteractorStyle);

int main(int argc, char* argv[])
{
    if (argc > 1 && std::string(argv[1]) == "--selfcheck")
        return selfCheck();

    MeshData mesh;

    MeshDataVtk test_mesh_data = MakeMeshDataVtk(mesh);

    vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(0.2, 0.3, 0.4);

    vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(600, 600);

    vtkSmartPointer<vtkRenderWindowInteractor> interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    interactor->SetRenderWindow(renderWindow);

    vtkSmartPointer<VertexPickInteractorStyle> style = vtkSmartPointer<VertexPickInteractorStyle>::New();
    interactor->SetInteractorStyle(style);

    // 创建MeshActor（点集由 actor 从 model_data.vertex_positions_ 自建）
    std::shared_ptr meshActor = std::make_shared<MeshActor>(renderer);
    // 加载模型数据
    meshActor->loadModelData(test_mesh_data);

    // 集成 VertexSelectorHighlight（演示组件单一，id 查询用恒等桩）
    StubMeshIdQuery id_query;
    vtkNew<vtkPartitionedDataSet> highlight_data;
    VertexSelectorHighlight selector(*renderer, *highlight_data, 0, MeshActorSelectOp(meshActor), 0, &id_query);
    style->SetSelectorHighlight(&selector);

    renderWindow->Render();
    interactor->Start();
    return 0;
}
