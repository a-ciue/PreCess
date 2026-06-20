#include "MakeMeshDataVtk.h"

#include "SelectorHighlight.h"
#include <vtkActor.h>
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
class EdgePickInteractorStyle : public vtkInteractorStyleTrackballCamera {
public:
    static EdgePickInteractorStyle* New();
    vtkTypeMacro(EdgePickInteractorStyle, vtkInteractorStyleTrackballCamera);

    void SetSelectorHighlight(EdgeSelectorHighlight* selector)
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
    EdgeSelectorHighlight* Selector = nullptr;
};

vtkStandardNewMacro(EdgePickInteractorStyle);

int main(int argc, char* argv[])
{
    MeshData mesh;

    MeshDataVtk test_mesh_data = MakeMeshDataVtk(mesh);

    vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(0.2, 0.3, 0.4);

    vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(600, 600);

    vtkSmartPointer<vtkRenderWindowInteractor> interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    interactor->SetRenderWindow(renderWindow);

    vtkSmartPointer<EdgePickInteractorStyle> style = vtkSmartPointer<EdgePickInteractorStyle>::New();
    interactor->SetInteractorStyle(style);

    vtkNew<vtkPoints> pts;
    pts->SetNumberOfPoints(static_cast<vtkIdType>(mesh.vertex_positions_.size()));
    for (size_t i = 0; i < mesh.vertex_positions_.size(); ++i) {
        pts->SetPoint(static_cast<vtkIdType>(i), mesh.vertex_positions_[i].data());
    }

    // 创建MeshActor
    std::shared_ptr meshActor = std::make_shared<MeshActor>(renderer, pts, true, ModelRenderMode::Face);
    // 加载模型数据
    meshActor->loadModelData(test_mesh_data);

    // 集成 EdgeSelectorHighlight
    EdgeSelectorHighlight selector(renderer);
    selector.setCurModelActor(MeshActorSelectOpFactory(meshActor));
    style->SetSelectorHighlight(&selector);

    renderWindow->Render();
    interactor->Start();
    return 0;
}
