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

    MeshDataVtk test_mesh_data = inputFile.empty()
        ? MakeMeshDataVtk(mesh)
        : MakeMeshDataVtkFromFile(inputFile, mesh);

    vtkNew<vtkRenderer> renderer;
    vtkNew<vtkRenderWindow> renderWindow;
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(1000, 700);
    renderWindow->SetWindowName("Complex MeshActor Multiple Solids Test");

    vtkNew<vtkRenderWindowInteractor> renderWindowInteractor;
    renderWindowInteractor->SetRenderWindow(renderWindow);

    vtkNew<vtkInteractorStyleTrackballCamera> style;
    renderWindowInteractor->SetInteractorStyle(style);

    vtkNew<vtkPoints> pts;
    pts->SetNumberOfPoints(static_cast<vtkIdType>(mesh.vertex_positions_.size()));
    for (size_t i = 0; i < mesh.vertex_positions_.size(); ++i) {
        pts->SetPoint(static_cast<vtkIdType>(i), mesh.vertex_positions_[i].data());
    }

    MeshActor meshActor(renderer, pts, true);
    meshActor.loadModelData(test_mesh_data);
    meshActor.setVisibility(true);
    meshActor.setRenderEdge(true);

    renderer->SetBackground(0.1, 0.2, 0.4);
    renderWindow->Render();
    renderWindowInteractor->Start();
}
