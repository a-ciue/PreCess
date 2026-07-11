#include <gmsh.h>

#include "GeometryData.h"
#include "IncrementalMeshTools.h"
#include "MeshData.h"
#include "ModelLayer.h"

#include <spdlog/spdlog.h>

#include <IFSelect_ReturnStatus.hxx>
#include <STEPControl_Reader.hxx>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <vtkActor.h>
#include <vtkCallbackCommand.h>
#include <vtkCellArray.h>
#include <vtkCommand.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>

// VTK 按键回调和示例主循环共享的运行状态。
struct AppContext {
    GeometryData geometry;
    GmshIncrementalMeshState gmshState;
    MeshData meshData;
    ModelLayer modelLayer;
    vtkSmartPointer<vtkPolyData> polyData;
    vtkRenderWindow* window {};
    std::size_t currentIndex { 0 };
    double meshSize { 10.0 };
};

// 示例加载 STEP，
static bool loadStepGeometry(const std::string& path, GeometryData& geometry, GeometryRegistry& registry)
{
    STEPControl_Reader reader;
    if (reader.ReadFile(path.c_str()) != IFSelect_RetDone)
        return false;

    reader.TransferRoots();
    geometry.rootShape = std::make_unique<TopoDS_Shape>(reader.OneShape());
    if (geometry.rootShape->IsNull())
        return false;

    geometry.ensureIndexBuilt(registry);
    return true;
}

// 根据当前 MeshData 的局部到全局点映射，生成“全局点 ID -> 本地点序号”的查询表。
static std::unordered_map<Index, std::size_t> buildGlobalToLocalPointMap(const MeshData& mesh)
{
    std::unordered_map<Index, std::size_t> globalToLocal;
    const auto& globalIds = mesh.local_to_global_;
    for (std::size_t i = 0; i < globalIds.size(); ++i) {
        globalToLocal[globalIds[i]] = i;
    }
    return globalToLocal;
}

// 从 MeshData 重建 vtkPolyData，避免示例程序依赖 app/render 层。
static void reloadPolyData(AppContext& ctx)
{
    auto points = vtkSmartPointer<vtkPoints>::New();
    for (const auto& p : ctx.meshData.vertex_positions_) {
        points->InsertNextPoint(p[0], p[1], p[2]);
    }

    auto polys = vtkSmartPointer<vtkCellArray>::New();
    auto globalToLocal = buildGlobalToLocalPointMap(ctx.meshData);

    for (std::size_t i = 0; i + 1 < ctx.meshData.face_vertices_offset_.size(); ++i) {
        std::size_t begin = ctx.meshData.face_vertices_offset_[i];
        std::size_t end = ctx.meshData.face_vertices_offset_[i + 1];
        vtkIdType count = static_cast<vtkIdType>(end - begin);
        if (count <= 0)
            continue;

        polys->InsertNextCell(count);
        for (std::size_t j = begin; j < end; ++j) {
            auto it = globalToLocal.find(ctx.meshData.face_vertices_[j]);
            if (it == globalToLocal.end()) {
                spdlog::warn("Missing local point for global id {}", ctx.meshData.face_vertices_[j]);
                polys->InsertCellPoint(0);
            } else {
                polys->InsertCellPoint(static_cast<vtkIdType>(it->second));
            }
        }
    }

    ctx.polyData->SetPoints(points);
    ctx.polyData->SetPolys(polys);
    ctx.polyData->Modified();
    ctx.window->Render();
}

// 清空已生成网格和 Gmsh 缓存，但保留已经加载的 CAD 形体。
static void resetGeneratedMesh(AppContext& ctx)
{
    ctx.meshData.init();
    ctx.gmshState = {};
    if (ctx.geometry.rootShape)
        ctx.geometry.ensureIndexBuilt(ctx.modelLayer.geomRegistry());
    ctx.currentIndex = 0;
    ctx.meshSize = IncrementalMeshTools::estimateMeshSize(ctx.geometry);
    reloadPolyData(ctx);
}

// 保存示例网格时，把全局点 ID 转回文件内的局部点编号。
static void saveMesh(const MeshData& mesh, const std::string& filename)
{
    if (mesh.vertex_positions_.empty()) {
        spdlog::warn("No mesh data to save.");
        return;
    }

    try {
        gmsh::initialize();
        gmsh::model::add("merged");
        int tag = gmsh::model::addDiscreteEntity(2);

        std::vector<std::size_t> nodeTags(mesh.vertex_positions_.size());
        std::vector<double> nodeCoords(mesh.vertex_positions_.size() * 3);
        auto globalToLocal = buildGlobalToLocalPointMap(mesh);

        for (std::size_t i = 0; i < mesh.vertex_positions_.size(); ++i) {
            nodeTags[i] = i + 1;
            nodeCoords[i * 3] = mesh.vertex_positions_[i][0];
            nodeCoords[i * 3 + 1] = mesh.vertex_positions_[i][1];
            nodeCoords[i * 3 + 2] = mesh.vertex_positions_[i][2];
        }
        gmsh::model::mesh::addNodes(2, tag, nodeTags, nodeCoords);

        std::vector<std::size_t> triTags, triNodes, quadTags, quadNodes;
        std::size_t elemTag = 1;
        for (std::size_t i = 0; i + 1 < mesh.face_vertices_offset_.size(); ++i) {
            std::size_t begin = mesh.face_vertices_offset_[i];
            std::size_t end = mesh.face_vertices_offset_[i + 1];
            std::size_t count = end - begin;
            if (count != 3 && count != 4)
                continue;

            auto& tags = count == 3 ? triTags : quadTags;
            auto& nodes = count == 3 ? triNodes : quadNodes;
            tags.push_back(elemTag++);
            for (std::size_t j = begin; j < end; ++j) {
                auto it = globalToLocal.find(mesh.face_vertices_[j]);
                if (it == globalToLocal.end()) {
                    spdlog::error("Missing local node for global id {}", mesh.face_vertices_[j]);
                    gmsh::finalize();
                    return;
                }
                nodes.push_back(it->second + 1);
            }
        }

        if (!triTags.empty())
            gmsh::model::mesh::addElementsByType(tag, 2, triTags, triNodes);
        if (!quadTags.empty())
            gmsh::model::mesh::addElementsByType(tag, 3, quadTags, quadNodes);

        gmsh::write(filename);
        spdlog::info("Saved: {}", std::filesystem::absolute(filename).string());
        gmsh::finalize();
    } catch (const std::exception& e) {
        spdlog::error("Save failed: {}", e.what());
        if (gmsh::isInitialized())
            gmsh::finalize();
    }
}

static void KeyPressCallback(vtkObject* caller, unsigned long, void* clientData, void*)
{
    auto* ctx = static_cast<AppContext*>(clientData);
    auto* interactor = static_cast<vtkRenderWindowInteractor*>(caller);
    std::string key = interactor->GetKeySym();

    std::size_t total = IncrementalMeshTools::faceCount(ctx->geometry);

    if (key == "space") {
        if (ctx->currentIndex >= total) {
            spdlog::info("All faces meshed.");
            return;
        }

        IncrementalMeshTools::GmshMeshParameters parameters;
        parameters.targetMeshSize = ctx->meshSize;

        auto result = IncrementalMeshTools::meshSingleFace(
            ctx->meshData,
            ctx->geometry,
            ctx->gmshState,
            ctx->modelLayer,
            ctx->currentIndex,
            ctx->meshSize,
            parameters);

        ctx->currentIndex++;

        if (result.success) {
            reloadPolyData(*ctx);
            if (ctx->meshSize < 50.0)
                ctx->meshSize *= 1.5;
            spdlog::info("nodes={}, cached_edges={}, next_size={:.4f}",
                ctx->meshData.vertex_positions_.size(),
                ctx->gmshState.meshedEdgeRefCounts.size(),
                ctx->meshSize);
        }
    } else if (key == "s" || key == "S") {
        saveMesh(ctx->meshData, "final_mesh.msh");
    } else if (key == "r" || key == "R") {
        resetGeneratedMesh(*ctx);
        spdlog::info("Reset mesh, size={:.4f}", ctx->meshSize);
    } else if (key == "plus" || key == "equal") {
        ctx->meshSize *= 2.0;
        spdlog::info("Size: {:.4f}", ctx->meshSize);
    } else if (key == "minus") {
        ctx->meshSize *= 0.5;
        spdlog::info("Size: {:.4f}", ctx->meshSize);
    } else if (key == "d" || key == "D") {
        if (ctx->currentIndex == 0) {
            spdlog::warn("No meshed face to delete.");
            return;
        }
        ctx->currentIndex--;
        if (IncrementalMeshTools::deleteFaceMesh(
                ctx->meshData, ctx->geometry, ctx->gmshState,
                ctx->modelLayer, ctx->currentIndex)) {
            reloadPolyData(*ctx);
        }
    } else if (key == "h" || key == "H") {
        spdlog::info("SPACE=mesh, S=save, R=reset, +/-=size, D=delete, H=help");
    }
}

int main(int argc, char* argv[])
{
    spdlog::set_level(spdlog::level::info);

    std::string path = argc > 1 ? argv[1] : "E:/VSProject/_models/models/airplane.stp";
    spdlog::info("Loading: {}", path);

    AppContext ctx;
    ctx.polyData = vtkSmartPointer<vtkPolyData>::New();
    ctx.meshData.init();

    if (!loadStepGeometry(path, ctx.geometry, ctx.modelLayer.geomRegistry())) {
        spdlog::error("Cannot import: {}", path);
        return 1;
    }
    ctx.meshSize = IncrementalMeshTools::estimateMeshSize(ctx.geometry);

    auto renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(0.1, 0.15, 0.2);

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(ctx.polyData);

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(0.85, 0.88, 0.92);
    actor->GetProperty()->EdgeVisibilityOn();
    actor->GetProperty()->SetEdgeColor(0.1, 0.2, 0.35);
    renderer->AddActor(actor);

    auto window = vtkSmartPointer<vtkRenderWindow>::New();
    window->AddRenderer(renderer);
    window->SetSize(1024, 768);
    window->SetWindowName("GmshMesh Test");
    ctx.window = window;

    auto interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    interactor->SetRenderWindow(window);

    auto cb = vtkSmartPointer<vtkCallbackCommand>::New();
    cb->SetCallback(KeyPressCallback);
    cb->SetClientData(&ctx);
    interactor->AddObserver(vtkCommand::KeyPressEvent, cb);

    window->Render();
    interactor->Start();
    return 0;
}
