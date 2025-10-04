#include "MakeMeshDataVtk.h"
#include "SelectorHighlight.h"
#include "spdlog/spdlog.h"

#include <vtkActor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>

// 自定义交互器，响应鼠标左键点击，驱动体元拾取
class SolidPickInteractorStyle : public vtkInteractorStyleTrackballCamera {
public:
    static SolidPickInteractorStyle* New();
    vtkTypeMacro(SolidPickInteractorStyle, vtkInteractorStyleTrackballCamera);

    void SetSelectorHighlight(SingleSolidSelectorHighlight* selector) { this->Selector = selector; }

    void OnLeftButtonDown() override {
        int* clickPos = this->GetInteractor()->GetEventPosition();
        if (Selector) {
            Selector->select(static_cast<double>(clickPos[0]), static_cast<double>(clickPos[1]));
        }
        vtkInteractorStyleTrackballCamera::OnLeftButtonDown();
    }

private:
    SingleSolidSelectorHighlight* Selector = nullptr;
};

vtkStandardNewMacro(SolidPickInteractorStyle);

int main(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::debug);

    // 使用内置示例数据（可根据 TestMeshActor 改为从文件导入）
    std::vector<std::array<double, 3>> vtk_points_;

    std::vector<unsigned char> vtk_solid_cell_types_;
    std::vector<Index> vtk_solid_cells_;
    std::vector<Index> vtk_solid_cells_offset_;
    std::vector<Index> vtk_solid_faces_;
    std::vector<Index> vtk_solid_faces_offset_;
    std::vector<Index> vtk_solid_face_locations_;
    std::vector<Index> vtk_solid_face_locations_offset_;

    std::vector<Index> vtk_face_cells_;
    std::vector<Index> vtk_face_cells_offset_;

    std::vector<Index> vtk_edge_cells_;

    MeshDataVtk test_mesh_data = MakeMeshDataVtk(
        vtk_points_,
        vtk_solid_cell_types_,
        vtk_solid_cells_,
        vtk_solid_cells_offset_,
        vtk_solid_faces_,
        vtk_solid_faces_offset_,
        vtk_solid_face_locations_,
        vtk_solid_face_locations_offset_,
        vtk_face_cells_,
        vtk_face_cells_offset_,
        vtk_edge_cells_);

    vtkNew<vtkRenderer> renderer;
    renderer->SetBackground(0.15, 0.2, 0.3);

    vtkNew<vtkRenderWindow> renderWindow;
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(800, 600);

    vtkNew<vtkRenderWindowInteractor> interactor;
    interactor->SetRenderWindow(renderWindow);

    vtkNew<SolidPickInteractorStyle> style;
    interactor->SetInteractorStyle(style);

    // 创建 MeshActor 并加载数据
    std::shared_ptr meshActor = std::make_shared<MeshActor>(renderer, true, ModelRenderMode::Face);
    meshActor->loadModelData(test_mesh_data);

    // 体元高亮选择器
    SingleSolidSelectorHighlight selector(renderer);
    selector.setCurModelActor(MeshActorSelectOpFactory(meshActor));
    style->SetSelectorHighlight(&selector);

    renderWindow->Render();
    interactor->Start();
    return 0;
}
