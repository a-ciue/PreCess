#include <gmsh.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include "IncrementalMeshTools.h"
#include "IncrementalMeshTools.cpp"

#include "SplineData.h"
#include "MeshData.h"
#include "MeshActor.h"
#include "ModelData.h"
#include "MakeMeshDataVtk.h" 
#include <spdlog/spdlog.h>
#include <vtkCallbackCommand.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>

// ================================================================
// 保存
// ================================================================
static void saveMesh(const MeshData& md, const std::string& filename)
{
    if (md.vertex_positions_.empty()) {
        spdlog::warn("No mesh data to save.");
        return;
    }
    try {
        gmsh::initialize();
        gmsh::model::add("merged");
        int tag = gmsh::model::addDiscreteEntity(2);

        std::vector<std::size_t> nt(md.vertex_positions_.size());
        std::vector<double> nc(md.vertex_positions_.size() * 3);
        for (size_t i = 0; i < md.vertex_positions_.size(); ++i) {
            nt[i] = i + 1;
            nc[i * 3] = md.vertex_positions_[i][0];
            nc[i * 3 + 1] = md.vertex_positions_[i][1];
            nc[i * 3 + 2] = md.vertex_positions_[i][2];
        }
        gmsh::model::mesh::addNodes(2, tag, nt, nc);

        std::vector<std::size_t> tt, tn, qt, qn;
        std::size_t ec = 1;
        for (size_t i = 0; i + 1 < md.face_vertices_offset_.size(); ++i) {
            size_t s = md.face_vertices_offset_[i];
            size_t e = md.face_vertices_offset_[i + 1];
            size_t c = e - s;
            if (c == 3) {
                tt.push_back(ec++);
                for (size_t j = s; j < e; ++j)
                    tn.push_back(md.face_vertices_[j] + 1);
            } else if (c == 4) {
                qt.push_back(ec++);
                for (size_t j = s; j < e; ++j)
                    qn.push_back(md.face_vertices_[j] + 1);
            }
        }
        if (!tt.empty())
            gmsh::model::mesh::addElementsByType(tag, 2, tt, tn);
        if (!qt.empty())
            gmsh::model::mesh::addElementsByType(tag, 3, qt, qn);

        gmsh::write(filename);
        spdlog::info("Saved: {}", std::filesystem::absolute(filename).string());
        gmsh::finalize();
    } catch (const std::exception& e) {
        spdlog::error("Save failed: {}", e.what());
        if (gmsh::isInitialized())
            gmsh::finalize();
    }
}

// ================================================================
// AppContext + 回调
// ================================================================
struct AppContext {
    SplineData* spline;
    MeshActor* actor;
    vtkRenderWindow* window;
    
    // 累积状态
    MeshData meshData;
    std::size_t currentIndex = 0;
    double meshSize = 10.0;

    void init(double estimatedSize)
    {
        meshData.clear();
        currentIndex = 0;
        meshSize = estimatedSize;

        meshData.face_vertices_offset_.push_back(0);
        if (meshData.solid_vertices_offset_.empty())
            meshData.solid_vertices_offset_.push_back(0);
        if (meshData.solid_faces_vertices_offset_.empty())
            meshData.solid_faces_vertices_offset_.push_back(0);
        if (meshData.solid_faces_offset_.empty())
            meshData.solid_faces_offset_.push_back(0);
    }
};

static void KeyPressCallback(vtkObject* caller, unsigned long,
    void* clientData, void*)
{
    auto* ctx = static_cast<AppContext*>(clientData);
    auto* interactor = static_cast<vtkRenderWindowInteractor*>(caller);
    std::string key = interactor->GetKeySym();

    std::size_t total = IncrementalMeshTools::faceCount(*ctx->spline);

    if (key == "space") {
        if (ctx->currentIndex >= total) {
            spdlog::info("All faces meshed!");
            return;
        }
        spdlog::info(">>> Face {}/{} (size={:.4f})",
            ctx->currentIndex + 1, total, ctx->meshSize);

        auto res = IncrementalMeshTools::meshSingleFace(
            ctx->meshData, *ctx->spline, ctx->currentIndex, ctx->meshSize);
        ctx->currentIndex++;

        if (res.success) {
            ctx->actor->loadModelData(MakeMeshDataVtk(ctx->meshData));
            ctx->window->Render();
        }

        if (ctx->meshSize < 50.0)
            ctx->meshSize *= 1.5;
        spdlog::info("  nodes={}, edges={}, next_size={:.4f}",
            ctx->meshData.vertex_positions_.size(),
            IncrementalMeshTools::meshedEdgeCount(*ctx->spline),
            ctx->meshSize);
    } else if (key == "s" || key == "S")
        saveMesh(ctx->meshData, "final_mesh.msh");
    else if (key == "r" || key == "R") {
        ctx->meshSize = IncrementalMeshTools::estimateMeshSize(*ctx->spline);
        spdlog::info("Clean vertices,Reset size: {:.4f}", ctx->meshSize);
    } else if (key == "plus" || key == "equal") {
        ctx->meshSize *= 2.0;
        spdlog::info("Size: {:.4f}", ctx->meshSize);
    } else if (key == "minus") {
        ctx->meshSize *= 0.5;
        spdlog::info("Size: {:.4f}", ctx->meshSize);
    } else if (key == "d" || key == "D") {
        if (ctx->currentIndex == 0) {
            spdlog::warn("没有可以删除的网格面了！");
        } else {
            ctx->currentIndex--;
            spdlog::info("Delete mesh for face: {}", ctx->currentIndex);
            
            if (ctx->currentIndex == 0) {
                ctx->init(ctx->meshSize);
            } else {
                IncrementalMeshTools::deleteFaceMesh(ctx->meshData, *ctx->spline, ctx->currentIndex);
            }
            
            ctx->actor->loadModelData(MakeMeshDataVtk(ctx->meshData));
            ctx->window->Render();
        }
    } else if (key == "h" || key == "H") {
        spdlog::info("SPACE=mesh, S=save, R=reset, +/-=size, D=delete, H=help");
    }
}

// ================================================================
// Main
// ================================================================
int main(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::info);

    SplineData spline;

    std::string path = (argc > 1)
        ? argv[1]
        : "E:/VSProject/_models/models/airplane.stp";

    spdlog::info("Loading: {}", path);

    if (!IncrementalMeshTools::initMeshing(path, spline)) {
        spdlog::error("Cannot import: {}", path);
        return 1;
    }

    auto renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(0.1, 0.15, 0.2);

    auto window = vtkSmartPointer<vtkRenderWindow>::New();
    window->AddRenderer(renderer);
    window->SetSize(1024, 768);
    window->SetWindowName("GmshMesh Test");

    auto interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    interactor->SetRenderWindow(window);

    auto actor = std::make_shared<MeshActor>(
        renderer, true, true, ModelRenderMode::Face);

    AppContext ctx{ &spline, actor.get(), window };
    ctx.init(IncrementalMeshTools::estimateMeshSize(spline));

    auto cb = vtkSmartPointer<vtkCallbackCommand>::New();
    cb->SetCallback(KeyPressCallback);
    cb->SetClientData(&ctx);
    interactor->AddObserver(vtkCommand::KeyPressEvent, cb);

    window->Render();
    interactor->Start();
    return 0;
}