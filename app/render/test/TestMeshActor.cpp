#include "Core.h"
#include "MakeMeshDataVtk.h"
#include "MeshActor.h"
#include <vtkCellType.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkNew.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <iostream>

int main(int argc, char** argv)
{
    // 可选：传入一个外部 vtk/vtu/vtp 文件路径
    std::string inputFile;
    if (argc > 1) {
        inputFile = argv[1];
        std::cout << "Try load file: " << inputFile << std::endl;
    }

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

    MeshDataVtk test_mesh_data = inputFile.empty()
        ? MakeMeshDataVtk(
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
            vtk_edge_cells_)
        : MakeMeshDataVtkFromFile(
            inputFile,
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
    vtkNew<vtkRenderWindow> renderWindow;
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(1000, 700);
    renderWindow->SetWindowName("Complex MeshActor Multiple Solids Test");

    vtkNew<vtkRenderWindowInteractor> renderWindowInteractor;
    renderWindowInteractor->SetRenderWindow(renderWindow);

    vtkNew<vtkInteractorStyleTrackballCamera> style;
    renderWindowInteractor->SetInteractorStyle(style);

    MeshActor meshActor(renderer, true, ModelRenderMode::Face);
    meshActor.loadModelData(test_mesh_data);
    meshActor.setRenderMode(ModelRenderMode::Face);
    meshActor.setVisibility(true);
    meshActor.setRenderEdge(true);

    renderer->SetBackground(0.1, 0.2, 0.4);
    renderWindow->Render();
    renderWindowInteractor->Start();
}
