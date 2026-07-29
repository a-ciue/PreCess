#include "MakeMeshDataVtk.h"
#include "SelectorHighlight.h"
#include <vtkPartitionedDataSet.h>
#include "spdlog/spdlog.h"

#include <vtkActor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPoints.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>

// 自定义交互器，响应鼠标左键点击，驱动体元拾取
class SolidPickInteractorStyle : public vtkInteractorStyleTrackballCamera {
public:
    static SolidPickInteractorStyle* New();
    vtkTypeMacro(SolidPickInteractorStyle, vtkInteractorStyleTrackballCamera);

    void SetSelectorHighlight(SolidSelectorHighlight* selector) { this->Selector = selector; }

    void OnLeftButtonDown() override
    {
        int* clickPos = this->GetInteractor()->GetEventPosition();
        if (Selector) {
            Selector->select(static_cast<double>(clickPos[0]), static_cast<double>(clickPos[1]));
        }
        vtkInteractorStyleTrackballCamera::OnLeftButtonDown();
    }

private:
    SolidSelectorHighlight* Selector = nullptr;
};

vtkStandardNewMacro(SolidPickInteractorStyle);

int main(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::debug);

    // 可选：传入一个外部 vtk/vtu/vtp 文件路径
    std::string inputFile;
    if (argc > 1) {
        inputFile = argv[1];
        spdlog::info("Try load file: {}", inputFile);
    }

    MeshData mesh;

    MeshDataVtk test_mesh_data = inputFile.empty()
        ? MakeMeshDataVtk(mesh)
        : MakeMeshDataVtkFromFile(inputFile, mesh);

    vtkNew<vtkRenderer> renderer;
    renderer->SetBackground(0.15, 0.2, 0.3);

    vtkNew<vtkRenderWindow> renderWindow;
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(800, 600);

    vtkNew<vtkRenderWindowInteractor> interactor;
    interactor->SetRenderWindow(renderWindow);

    vtkNew<SolidPickInteractorStyle> style;
    interactor->SetInteractorStyle(style);

    vtkNew<vtkPoints> pts;
    pts->SetNumberOfPoints(static_cast<vtkIdType>(mesh.vertex_positions_.size()));
    for (size_t i = 0; i < mesh.vertex_positions_.size(); ++i) {
        pts->SetPoint(static_cast<vtkIdType>(i), mesh.vertex_positions_[i].data());
    }

    // 创建 MeshActor 并加载数据
    std::shared_ptr meshActor = std::make_shared<MeshActor>(renderer, pts);
    meshActor->loadModelData(test_mesh_data);

    // 体元高亮选择器
    vtkNew<vtkPartitionedDataSet> highlight_data;
    SolidSelectorHighlight selector(*renderer, *highlight_data, 0, MeshActorSelectOp(meshActor));
    style->SetSelectorHighlight(&selector);

    renderWindow->Render();
    interactor->Start();
    return 0;
}
