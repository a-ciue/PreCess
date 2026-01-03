#include "MakeMeshData.h"
#include "MakeMeshDataVtk.h"
#include "SelectorHighlight.h"
#include <spdlog/spdlog.h>
#include <vtkCallbackCommand.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
static void KeyPressCallbackFunc(vtkObject* caller, unsigned long eventId, void* client_data, [[maybe_unused]] void* call_data)
{
    if (eventId == vtkCommand::KeyPressEvent) {
        MeshActor* mesh_actor_ptr = static_cast<MeshActor*>(client_data);
        vtkRenderWindowInteractor* interactor = static_cast<vtkRenderWindowInteractor*>(caller);
        const char* key = interactor->GetKeySym();
        spdlog::info("Key pressed: [{}]", key);

        if (strcmp(key, "1") == 0 || strcmp(key, "KP_1") == 0) {
            mesh_actor_ptr->setAttriMode("VertexScalar", MeshActor::SCALAR, MeshActor::VERTEX);
            spdlog::info("Switched to Vertex Scalar: VertexScalar");
        } else if (strcmp(key, "2") == 0 || strcmp(key, "KP_2") == 0) {
            mesh_actor_ptr->setAttriMode("vertex_color_3", MeshActor::RGB, MeshActor::VERTEX);
            spdlog::info("Switched to Vertex Color: vertex_color_3");
        } else if (strcmp(key, "4") == 0 || strcmp(key, "KP_4") == 0) {
            mesh_actor_ptr->setAttriMode("FaceScalar", MeshActor::SCALAR, MeshActor::FACE);
            spdlog::info("Switched to Face Scalar: FaceScalar");
        } else if (strcmp(key, "5") == 0 || strcmp(key, "KP_5") == 0) {
            mesh_actor_ptr->setAttriMode("face_color_3", MeshActor::RGB, MeshActor::FACE);
            spdlog::info("Switched to Face Color: face_color_3");
        } else if (strcmp(key, "6") == 0 || strcmp(key, "KP_6") == 0) {
            mesh_actor_ptr->setAttriMode("vertex_press_3", MeshActor::VECTOR, MeshActor::VERTEX);
            spdlog::info("Switched to Vertex Vector: vertex_press_3");
        } else if (strcmp(key, "7") == 0 || strcmp(key, "KP_7") == 0) {
            mesh_actor_ptr->setAttriMode("vertex_normal_3", MeshActor::VECTOR, MeshActor::VERTEX);
            spdlog::info("Switched to Vertex Vector: vertex_normal_3");
        } else if (strcmp(key, "8") == 0 || strcmp(key, "KP_8") == 0) {
            mesh_actor_ptr->setAttriMode("face_normal_3", MeshActor::VECTOR, MeshActor::FACE);
            spdlog::info("Switched to Face Vector: face_normal_3");
        } else if (strcmp(key, "0") == 0 || strcmp(key, "KP_0") == 0) {
            mesh_actor_ptr->cancelActiveAttribute();
            spdlog::info("cancelActiveAttribute");
        } else if (strcmp(key, "t") == 0) {
            mesh_actor_ptr->setAttriMode("VertexScalar", MeshActor::SCALAR, MeshActor::VERTEX, "", 0.3, std::make_pair(2.0, 6.0));
            spdlog::info("setScalarRange(2, 6)");
        } else if (strcmp(key, "y") == 0) {
            mesh_actor_ptr->setAttriMode("VertexScalar", MeshActor::SCALAR, MeshActor::VERTEX, "", 0.3, std::nullopt);
            spdlog::info("resetScalarRange()");
        } else if (strcmp(key, "u") == 0) {
            mesh_actor_ptr->setAttriMode("vertex_press_3", MeshActor::VECTOR, MeshActor::VERTEX, "", 0.5, std::nullopt);
            spdlog::info("setGlyph3DScaleFactor(0.5)");
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
    vtkSmartPointer<vtkCallbackCommand> key_press_callback = vtkSmartPointer<vtkCallbackCommand>::New();
    key_press_callback->SetCallback(KeyPressCallbackFunc);
    key_press_callback->SetClientData(meshActor.get());
    if (interactor->AddObserver(vtkCommand::KeyPressEvent, key_press_callback) == 0) {
        spdlog::error("Failed to add key press observer!");
    }

    spdlog::info("Press '1' for Vertex Scalar (VertexScalar), '2' for Vertex Color (vertex_color_3)");
    spdlog::info("Press '4' for Face Scalar (FaceScalar), '5' for Face Color (face_color_3)");
    spdlog::info("Press '6' for Vertex Vector (vertex_press_3), '7' for Vertex Normal (vertex_normal_3)");
    spdlog::info("Press '8' for Face Normal (face_normal_3)");
    spdlog::info("Press '0' to cancel active scalar attribute, '9' to cancel active vector attribute");
    spdlog::info("Press 't' to set scalar range, 'y' to reset scalar range, 'u' to set glyph3D scale factor");
    renderWindow->Render();
    meshActor2->setAttriMode("vertex_uv_2", MeshActor::UV, MeshActor::VERTEX, argv[1]);
    interactor->Start();

    return 0;
}