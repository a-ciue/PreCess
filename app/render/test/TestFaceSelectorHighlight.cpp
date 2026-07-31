#include "MakeMeshDataVtk.h"

#include "SelectorHighlight.h"
#include <vtkPartitionedDataSet.h>
#include <vtkActor.h>
#include <vtkNew.h>
#include <vtkCell.h>
#include <vtkCellArray.h>
#include <vtkCommand.h>
#include <vtkGeometryFilter.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkPoints.h>
#include <vtkPolyDataMapper.h>
#include <vtkPolygon.h>
#include <vtkProperty.h>
#include <vtkQuad.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkTriangle.h>
#include <vtkUnstructuredGrid.h>
#include <vtkWin32OpenGLRenderWindow.h>

// 自定义交互器，响应鼠标左键点击
class FacePickInteractorStyle : public vtkInteractorStyleTrackballCamera {
public:
    static FacePickInteractorStyle* New();
    vtkTypeMacro(FacePickInteractorStyle, vtkInteractorStyleTrackballCamera);

    void SetSelectorHighlight(FaceSelectorHighlight* selector)
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
    FaceSelectorHighlight* Selector = nullptr;
};

vtkStandardNewMacro(FacePickInteractorStyle);

int main(int argc, char* argv[])
{
    MeshData mesh;
    std::vector<Index> point_gids; //> 全局点 id（iota 恒等），须与 test_mesh_data 同生命周期

    MeshDataVtk test_mesh_data = MakeMeshDataVtk(mesh, point_gids);

    vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(0.2, 0.3, 0.4);

    vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(600, 600);

    vtkSmartPointer<vtkRenderWindowInteractor> interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    interactor->SetRenderWindow(renderWindow);

    vtkSmartPointer<FacePickInteractorStyle> style = vtkSmartPointer<FacePickInteractorStyle>::New();
    interactor->SetInteractorStyle(style);

    // 创建MeshActor（点集由 actor 从 model_data.vertex_positions_ 自建）
    std::shared_ptr meshActor = std::make_shared<MeshActor>(renderer);
    // 加载模型数据
    meshActor->loadModelData(test_mesh_data);

    // 集成 FaceSelectorHighlight
    vtkNew<vtkPartitionedDataSet> highlight_data;
    FaceSelectorHighlight selector(*renderer, *highlight_data, 0, MeshActorSelectOp(meshActor));
    style->SetSelectorHighlight(&selector);

    renderWindow->Render();
    interactor->Start();
    return 0;
}
