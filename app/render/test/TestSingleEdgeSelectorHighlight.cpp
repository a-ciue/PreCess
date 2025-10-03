#include "MakeMeshDataVtk.h"

#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkPolyDataMapper.h>
#include <vtkActor.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkCommand.h>
#include <vtkProperty.h>
#include <vtkWin32OpenGLRenderWindow.h>
#include <vtkGeometryFilter.h>
#include <vtkQuad.h>
#include <vtkTriangle.h>
#include <vtkPolygon.h>
#include <vtkCell.h>
#include "SelectorHighlight.h"

// 自定义交互器，响应鼠标左键点击
class EdgePickInteractorStyle : public vtkInteractorStyleTrackballCamera {
public:
    static EdgePickInteractorStyle* New();
    vtkTypeMacro(EdgePickInteractorStyle, vtkInteractorStyleTrackballCamera);

    void SetSelectorHighlight(SingleEdgeSelectorHighlight* selector) {
        this->Selector = selector;
    }
    void OnLeftButtonDown() override {
        int* clickPos = this->GetInteractor()->GetEventPosition();
        if (Selector) {
            Selector->select(static_cast<double>(clickPos[0]), static_cast<double>(clickPos[1]));
        }
        vtkInteractorStyleTrackballCamera::OnLeftButtonDown();
    }
private:
    SingleEdgeSelectorHighlight* Selector = nullptr;
};

vtkStandardNewMacro(EdgePickInteractorStyle);

int main(int argc, char* argv[]) {
    std::vector<std::array<double, 3>> vtk_points_;

    std::vector<unsigned char> vtk_solid_cell_types_;
    std::vector<Index> vtk_solid_cells_;
    std::vector<Index> vtk_solid_cells_offset_;
    std::vector<Index> vtk_solid_faces_;
    std::vector<Index> vtk_solid_faces_offset_;
    std::vector<Index> vtk_solid_face_locations_;
    std::vector<Index> vtk_solid_face_locations_offset_;

    std::vector<Index> vtk_face_cells_; //> 表示面顶点索引的数组
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
        vtk_face_cells_, //> 表示面顶点索引的数组
        vtk_face_cells_offset_,
        vtk_edge_cells_ //> 表示边顶点索引的数组
    );


    vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(0.2, 0.3, 0.4);

    vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(600, 600);

    vtkSmartPointer<vtkRenderWindowInteractor> interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    interactor->SetRenderWindow(renderWindow);

    vtkSmartPointer<EdgePickInteractorStyle> style = vtkSmartPointer<EdgePickInteractorStyle>::New();
    interactor->SetInteractorStyle(style);


    // 创建MeshActor
    std::shared_ptr meshActor = std::make_shared<MeshActor>(renderer, true, ModelRenderMode::Face);
    // 加载模型数据
    meshActor->loadModelData(test_mesh_data);

    // 集成 SingleEdgeSelectorHighlight
    SingleEdgeSelectorHighlight selector(renderer);
    selector.setCurModelActor(MeshActorSelectOp(meshActor));
    style->SetSelectorHighlight(&selector);

    renderWindow->Render();
    interactor->Start();
    return 0;
}
