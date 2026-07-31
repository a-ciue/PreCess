#include "Core.h"
#include "MakeMeshDataVtk.h"
#include "MeshActor.h"
#include <vtkCellType.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkNew.h>
#include <vtkPoints.h>
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

    MeshData mesh;
    std::vector<Index> point_gids; //> 全局点 id（iota 恒等），须与 test_mesh_data 同生命周期

    MeshDataVtk test_mesh_data = inputFile.empty()
        ? MakeMeshDataVtk(mesh, point_gids)
        : MakeMeshDataVtkFromFile(inputFile, mesh, point_gids);

    vtkNew<vtkRenderer> renderer;
    vtkNew<vtkRenderWindow> renderWindow;
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(1000, 700);
    renderWindow->SetWindowName("Complex MeshActor Multiple Solids Test");

    vtkNew<vtkRenderWindowInteractor> renderWindowInteractor;
    renderWindowInteractor->SetRenderWindow(renderWindow);

    vtkNew<vtkInteractorStyleTrackballCamera> style;
    renderWindowInteractor->SetInteractorStyle(style);

    MeshActor meshActor(renderer);
    meshActor.loadModelData(test_mesh_data);
    meshActor.setVisibility(true);

    renderer->SetBackground(0.1, 0.2, 0.4);
    renderWindow->Render();
    renderWindowInteractor->Start();
}
