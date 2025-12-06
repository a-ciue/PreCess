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
#include <MakeMeshData.h>
#include <vtkCallbackCommand.h> 

static void KeyPressCallbackFunc(vtkObject* caller, unsigned long eventId, void* clientData, void* callData)
{
    if (eventId == vtkCommand::KeyPressEvent) {
        std::shared_ptr<MeshActor>* meshActorPtr = static_cast<std::shared_ptr<MeshActor>*>(clientData);
        vtkRenderWindowInteractor* interactor = static_cast<vtkRenderWindowInteractor*>(caller);
        const char* key = interactor->GetKeySym();
        std::cout << " Key pressed: [" << key << "]" << std::endl;

        if (strcmp(key, "1") == 0 || strcmp(key, "KP_1") == 0) {
            (*meshActorPtr)->setActiveScalarAttribute("VertexScalar", MeshActor::VERTEX);
            std::cout << "Switched to Vertex Scalar: VertexScalar" << std::endl;
        } else if (strcmp(key, "2") == 0 || strcmp(key, "KP_2") == 0) {
            (*meshActorPtr)->setActiveScalarAttribute("vertex_color_3", MeshActor::VERTEX);
            std::cout << "Switched to Vertex Vector: vertex_color_3" << std::endl;
        }
        else if (strcmp(key, "4") == 0 || strcmp(key, "KP_4") == 0) {
            (*meshActorPtr)->setActiveScalarAttribute("FaceScalar", MeshActor::FACE);
            std::cout << "Switched to Face Scalar: FcaeScalar" << std::endl;
        } else if (strcmp(key, "5") == 0 || strcmp(key, "KP_5") == 0) {
            (*meshActorPtr)->setActiveScalarAttribute("face_color_3", MeshActor::FACE);
            std::cout << "Switched to Face Scalar: face_color_3" << std::endl;
        } else if (strcmp(key, "6") == 0 || strcmp(key, "KP_6") == 0) {
            (*meshActorPtr)->setActiveVectorAttribute("vertex_press_3", MeshActor::VERTEX);
            std::cout << "Switched to Vertex Vector: vertex_press_3" << std::endl;
        } else if (strcmp(key, "7") == 0 || strcmp(key, "KP_7") == 0) {
            (*meshActorPtr)->setActiveVectorAttribute("vertex_normal_3", MeshActor::VERTEX);
            std::cout << "Switched to Vertex Vector: vertex_normal_3" << std::endl;
        }  else if (strcmp(key, "8") == 0 || strcmp(key, "KP_7") == 0) {
            (*meshActorPtr)->setActiveVectorAttribute("face_normal_3", MeshActor::FACE);
            std::cout << "Switched to Vertex Vector: face_normal_3" << std::endl;
        } else if (strcmp(key, "0") == 0 || strcmp(key, "KP_0") == 0) {
            (*meshActorPtr)->cancelActiveAttribute();
            std::cout << "cancelActiveAttribute" << std::endl;
        } 
    }
}

int main(int argc, char* argv[])
{
    // 使用 MakeMeshDataWithAtri() 创建带属性的 MeshData
    MeshData mesh = MakeMeshDataWithAtri();
    MeshData mesh2 = MakeMeshDataWithUV();
    MeshDataVtk test_mesh_data = MakeMeshDataVtk(mesh);
    MeshDataVtk test_mesh_data2 = MakeMeshDataVtk(mesh2);

    vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(0.2, 0.3, 0.4);

    vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(600, 600);

    vtkSmartPointer<vtkRenderWindowInteractor> interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    interactor->SetRenderWindow(renderWindow);

    // 创建 MeshActor
    std::shared_ptr<MeshActor> meshActor = std::make_shared<MeshActor>(renderer, true, true, ModelRenderMode::Face);
    meshActor->loadModelData(test_mesh_data);

    std::shared_ptr<MeshActor> meshActor2 = std::make_shared<MeshActor>(renderer, true, true, ModelRenderMode::Face);
    meshActor2->loadModelData(test_mesh_data2);
    // 按键交互逻辑
    vtkSmartPointer<vtkCallbackCommand> keyPressCallback = vtkSmartPointer<vtkCallbackCommand>::New();
    keyPressCallback->SetCallback(KeyPressCallbackFunc);
    keyPressCallback->SetClientData(&meshActor);
    if (interactor->AddObserver(vtkCommand::KeyPressEvent, keyPressCallback) == 0) {
        std::cerr << "Failed to add key press observer!" << std::endl;
    }
 
    std::cout << "Press '1' for Vertex Scalar (VertexScalar), '2' for Vertex Color (vertex_color_3)" << std::endl;
    std::cout << "Press '4' for Face Scalar (FaceScalar), '5' for Face Color (face_color_3)" << std::endl;
    std::cout << "Press '6' for Vertex Vector (vertex_press_3), '7' for Vertex Normal (vertex_normal_3)" << std::endl;
    std::cout << "Press '8' for Face Normal (face_normal_3)" << std::endl;
    std::cout << "Press '0' to cancel active scalar attribute, '9' to cancel active vector attribute" << std::endl;
    renderWindow->Render();
    meshActor2->setAttriMode(" ", MeshActor::UV, MeshActor::VERTEX, "E:/MeshProjects/Project_Harmonic/data/texture_checker.bmp");

    interactor->Start();




    return 0;
}