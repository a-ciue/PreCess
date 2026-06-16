#include "renderStrategy/AttriRenderStrategyRGB.h"
#include "renderStrategy/AttriRenderStrategyScalar.h"
#include "renderStrategy/AttriRenderStrategyUV.h"
#include "renderStrategy/AttriRenderStrategyVector.h"
#include "renderStrategy/AttributeCommon.h"
#include "MakeMeshData.h"
#include "MakeMeshDataVtk.h"
#include <spdlog/spdlog.h>
#include <vtkCallbackCommand.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
static void KeyPressCallbackFunc(vtkObject* caller, unsigned long eventId, void* client_data, [[maybe_unused]] void* call_data)
{
    if (eventId == vtkCommand::KeyPressEvent) {
        MeshActor* mesh_actor_ptr = static_cast<MeshActor*>(client_data);
        vtkRenderWindowInteractor* interactor = static_cast<vtkRenderWindowInteractor*>(caller);
        const char* key = interactor->GetKeySym();
        spdlog::info("Key pressed: [{}]", key);

        std::map<std::string, std::any> args;

        if (strcmp(key, "1") == 0 || strcmp(key, "KP_1") == 0) {
            mesh_actor_ptr->setRenderStrategy(std::make_unique<AttriRenderStrategyScalar>());
            mesh_actor_ptr->renderAttribute("VertexScalar", args);
            spdlog::info("Switched to Vertex Scalar: VertexScalar");
        } else if (strcmp(key, "2") == 0 || strcmp(key, "KP_2") == 0) {
            mesh_actor_ptr->setRenderStrategy(std::make_unique<AttriRenderStrategyRGB>());
            mesh_actor_ptr->renderAttribute("vertex_color_3", args);
            spdlog::info("Switched to Vertex Color: vertex_color_3");
        } else if (strcmp(key, "4") == 0 || strcmp(key, "KP_4") == 0) {
            mesh_actor_ptr->setRenderStrategy(std::make_unique<AttriRenderStrategyScalar>());
            mesh_actor_ptr->renderAttribute("FaceScalar", args);
            spdlog::info("Switched to Face Scalar: FaceScalar");
        } else if (strcmp(key, "5") == 0 || strcmp(key, "KP_5") == 0) {
            mesh_actor_ptr->setRenderStrategy(std::make_unique<AttriRenderStrategyRGB>());
            mesh_actor_ptr->renderAttribute("face_color_3", args);
            spdlog::info("Switched to Face Color: face_color_3");
        } else if (strcmp(key, "6") == 0 || strcmp(key, "KP_6") == 0) {
            mesh_actor_ptr->setRenderStrategy(std::make_unique<AttriRenderStrategyVector>());
            mesh_actor_ptr->renderAttribute("vertex_press_3", args);
            spdlog::info("Switched to Vertex Vector: vertex_press_3");
        } else if (strcmp(key, "7") == 0 || strcmp(key, "KP_7") == 0) {
            mesh_actor_ptr->setRenderStrategy(std::make_unique<AttriRenderStrategyVector>());
            mesh_actor_ptr->renderAttribute("vertex_normal_3", args);
            spdlog::info("Switched to Vertex Vector: vertex_normal_3");
        } else if (strcmp(key, "8") == 0 || strcmp(key, "KP_8") == 0) {
            mesh_actor_ptr->setRenderStrategy(std::make_unique<AttriRenderStrategyVector>());
            mesh_actor_ptr->renderAttribute("face_normal_3", args);
            spdlog::info("Switched to Face Vector: face_normal_3");
        } else if (strcmp(key, "0") == 0 || strcmp(key, "KP_0") == 0) {
            mesh_actor_ptr->cancelActiveAttribute();
            spdlog::info("cancelActiveAttribute");
        } else if (strcmp(key, "t") == 0) {
            mesh_actor_ptr->setRenderStrategy(std::make_unique<AttriRenderStrategyScalar>());
            args["scalar_range"] = std::vector<double> { 2.0, 6.0 };
            mesh_actor_ptr->renderAttribute("VertexScalar", args);
            spdlog::info("setScalarRange(2, 6)");
        } else if (strcmp(key, "y") == 0) {
            mesh_actor_ptr->setRenderStrategy(std::make_unique<AttriRenderStrategyScalar>());
            mesh_actor_ptr->renderAttribute("VertexScalar", args);
            spdlog::info("resetScalarRange()");
        } else if (strcmp(key, "u") == 0) {
            mesh_actor_ptr->setRenderStrategy(std::make_unique<AttriRenderStrategyVector>());
            args["glyph_scale"] = 0.5;
            mesh_actor_ptr->renderAttribute("vertex_press_3", args);
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

    vtkNew<vtkPoints> pts;
    pts->SetNumberOfPoints(static_cast<vtkIdType>(mesh.vertex_positions_.size()));
    for (size_t i = 0; i < mesh.vertex_positions_.size(); ++i) {
        pts->SetPoint(static_cast<vtkIdType>(i), mesh.vertex_positions_[i].data());
    }

    vtkNew<vtkPoints> pts2;
    pts2->SetNumberOfPoints(static_cast<vtkIdType>(mesh2.vertex_positions_.size()));
    for (size_t i = 0; i < mesh2.vertex_positions_.size(); ++i) {
        pts2->SetPoint(static_cast<vtkIdType>(i), mesh2.vertex_positions_[i].data());
    }

    // 创建 MeshActor
    std::shared_ptr<MeshActor> meshActor = std::make_shared<MeshActor>(renderer, pts, true, ModelRenderMode::Face);
    meshActor->loadModelData(test_mesh_data);

    std::shared_ptr<MeshActor> meshActor2 = std::make_shared<MeshActor>(renderer, pts2, true, ModelRenderMode::Face);
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
    if (argc > 1) {
        std::map<std::string, std::any> args;
        args["texture_path"] = std::string(argv[1]);
        meshActor2->setRenderStrategy(std::make_unique<AttriRenderStrategyUV>());
        meshActor2->renderAttribute("vertex_uv_2", args);
    } else {
        spdlog::error("Texture path not provided");
    }
    interactor->Start();

    return 0;
}