/**
 * @file TestInteractionService.cpp
 * @brief 通用视口交互服务的独立测试：拾取转发、标注拉取、结果回调与生命周期
 *
 * 用 FakeInteraction（填充 InteractionState 的假交互）替代真实插件，
 * 使交互服务不依赖任何具体功能插件即可独立验证（含程序化拾取自检）。
 */
#include "MakeMeshDataVtk.h"
#include "InteractionService.h"
#include "InteractionState.h"
#include "GeometryActorManager.h"
#include "GeometryDataVtk.h"
#include "GeometryRegistry.h"
#include "SelectManager.h"
#include "GeometrySubshapeIndex.h"
#include "MeshActorManager.h"

#include <BRepPrimAPI_MakeBox.hxx>
#include <BRep_Tool.hxx>
#include <IVtkTools_ShapePicker.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Pnt.hxx>

#include <spdlog/spdlog.h>
#include <vtkInteractorObserver.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>

#include <string>
#include <vector>

using systems::interaction::AnnotationBatch;
using systems::interaction::PickInfo;

namespace {

//! @brief 记录全部交互调用的假交互状态：替代真实插件验证服务契约
struct FakeInteraction {
    systems::interaction::InteractionState state;

    int activate_count = 0;
    int deactivate_count = 0;
    int hover_count = 0;
    std::vector<PickInfo> picks;

    FakeInteraction()
    {
        state.on_activate = [this] { ++activate_count; };
        state.on_deactivate = [this] { ++deactivate_count; };
        state.on_pick = [this](const PickInfo& pick) {
            picks.push_back(pick);
            return true;
        };
        state.on_hover = [this](const PickInfo&) {
            ++hover_count;
            return true;
        };
        state.annotations.points.push_back({ { 0.0, 0.0, 0.0 } }); //> 一个标注点，验证服务拉取
    }
};

//! @brief 与应用一致的事件源：左键确认拾取，悬停预览
class PickInteractorStyle : public vtkInteractorStyleTrackballCamera {
public:
    static PickInteractorStyle* New();
    vtkTypeMacro(PickInteractorStyle, vtkInteractorStyleTrackballCamera);

    void SetInteractionService(InteractionService* service) { service_ = service; }

    void OnLeftButtonUp() override
    {
        int* pos = this->GetInteractor()->GetEventPosition();
        if (service_ && service_->hasActiveState()) {
            service_->pick(pos[0], pos[1]);
            this->GetInteractor()->Render();
        }
        vtkInteractorStyleTrackballCamera::OnLeftButtonUp();
    }

    void OnMouseMove() override
    {
        vtkInteractorStyleTrackballCamera::OnMouseMove();
        if (service_ && service_->hasActiveState() && this->State == VTKIS_NONE) {
            int* pos = this->GetInteractor()->GetEventPosition();
            service_->hover(pos[0], pos[1]);
            this->GetInteractor()->Render();
        }
    }

private:
    InteractionService* service_ {};
};

vtkStandardNewMacro(PickInteractorStyle);

int g_failures = 0;
void check(bool cond, const std::string& name)
{
    spdlog::info("{}: {}", cond ? "PASS" : "FAIL", name);
    if (!cond)
        ++g_failures;
}

//! @brief 世界坐标投到显示坐标后驱动一次 pick
void pickWorld(InteractionService& service, vtkRenderer* renderer, const std::array<double, 3>& wp)
{
    double display[3];
    vtkInteractorObserver::ComputeWorldToDisplay(renderer, wp[0], wp[1], wp[2], display);
    service.pick(display[0], display[1]);
}

} // namespace

int main(int argc, char* argv[])
{
    MeshData mesh;
    std::vector<Index> point_gids; //> 全局点 id（iota 恒等），须与 test_mesh_data 同生命周期
    MeshDataVtk test_mesh_data = MakeMeshDataVtk(mesh, point_gids);

    vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
    renderer->SetBackground(0.2, 0.3, 0.4);

    vtkSmartPointer<vtkRenderWindow> renderWindow = vtkSmartPointer<vtkRenderWindow>::New();
    renderWindow->AddRenderer(renderer);
    renderWindow->SetSize(600, 600);

    // 叠加渲染层（与应用一致：标注置顶，不被模型遮挡）
    renderWindow->SetNumberOfLayers(2);
    vtkSmartPointer<vtkRenderer> overlay_renderer = vtkSmartPointer<vtkRenderer>::New();
    overlay_renderer->SetLayer(1);
    overlay_renderer->InteractiveOff();
    overlay_renderer->SetActiveCamera(renderer->GetActiveCamera());
    renderWindow->AddRenderer(overlay_renderer);

    vtkSmartPointer<vtkRenderWindowInteractor> interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    interactor->SetRenderWindow(renderWindow);

    vtkSmartPointer<PickInteractorStyle> style = vtkSmartPointer<PickInteractorStyle>::New();
    interactor->SetInteractorStyle(style);

    MeshActorManager mesh_manager;
    mesh_manager.bindRender(renderer.GetPointer());

    GeometryActorManager geometry_manager;
    geometry_manager.bindRender(renderer.GetPointer());

    // 与应用一致：几何顶点吸附经选择系统封装接口完成，不接触 picker
    SelectManager sel_mgr(*renderer, mesh_manager.op(), geometry_manager.op());

    FakeInteraction fake;
    InteractionService service(*renderer, *overlay_renderer, mesh_manager.op(), sel_mgr);

    // 拾取列表在服务构造时登记观察，网格须在此之后加载才会进入拾取列表
    // （与应用一致：服务于 initializeVTK 创建，模型其后加载）
    mesh_manager.loadMesh(0, test_mesh_data, renderer.GetPointer());

    // 交互状态经 provider 提供：开关开启前无激活状态，pick 不转发
    bool interaction_enabled = false;
    service.state_provider = [&]() -> systems::interaction::InteractionState* {
        return interaction_enabled ? &fake.state : nullptr;
    };
    style->SetInteractionService(&service);

    renderer->ResetCamera();
    renderWindow->Render();

    check(!service.hasActiveState(), "开关关闭时无激活交互");
    pickWorld(service, renderer.GetPointer(), mesh.vertex_positions_[0]);
    check(fake.picks.empty(), "无激活状态时 pick 不转发");

    // 开启开关：首次 pick 触发上线（on_activate + 几何吸附切换）
    interaction_enabled = true;
    check(service.hasActiveState(), "开关开启后存在激活交互");
    pickWorld(service, renderer.GetPointer(), mesh.vertex_positions_[0]);
    check(fake.activate_count == 1, "上线时调用 onActivate 一次");
    renderWindow->Render();
    check(fake.picks.size() == 1, "命中网格顶点后 onPick 被调用一次");
    if (!fake.picks.empty()) {
        const PickInfo& p = fake.picks.front();
        check(p.valid, "PickInfo.valid 为 true");
        check(p.mesh_id == 0, "PickInfo.mesh_id 为全局点 id（iota 恒等填充）");
        check(p.geom_id < 0, "网格拾取不填 geom_id");
    }

    // ---- 悬停转发 ----
    const int hover_before = fake.hover_count;
    double display[3];
    vtkInteractorObserver::ComputeWorldToDisplay(renderer.GetPointer(),
        mesh.vertex_positions_[1][0], mesh.vertex_positions_[1][1], mesh.vertex_positions_[1][2], display);
    service.hover(display[0], display[1]);
    check(fake.hover_count == hover_before + 1, "hover 转发到处理器");

    // ---- 空处拾取：未命中不调用 onPick ----
    const std::size_t picks_before = fake.picks.size();
    service.pick(5000, 5000); // 窗口外坐标，拾取器必然未命中
    check(fake.picks.size() == picks_before, "未命中吸附点时不调用 onPick");

    // ---- 几何（OCC）顶点拾取：geom_id 填充 ----
    {
        TopoDS_Shape box = BRepPrimAPI_MakeBox(gp_Pnt(500.0, 500.0, 0.0), 1000.0, 800.0, 600.0).Shape();
        GeometryRegistry geom_reg;
        GeometrySubshapeIndex geom_index;
        geom_index.build(box, geom_reg);
        const Index geom_component_id = 999;
        GeometryDataVtk geom_data { box, geom_component_id, &geom_index };
        geometry_manager.loadGeometry(geom_data);

        std::vector<std::array<double, 3>> box_vertices;
        for (TopExp_Explorer exp(box, TopAbs_VERTEX); exp.More(); exp.Next()) {
            const gp_Pnt p = BRep_Tool::Pnt(TopoDS::Vertex(exp.Current()));
            box_vertices.push_back({ p.X(), p.Y(), p.Z() });
        }

        renderer->ResetCamera();
        renderWindow->Render();

        const std::size_t before = fake.picks.size();
        pickWorld(service, renderer.GetPointer(), box_vertices[0]);
        renderWindow->Render();
        check(fake.picks.size() == before + 1, "命中几何顶点后 onPick 被调用");
        if (fake.picks.size() > before) {
            const PickInfo& p = fake.picks.back();
            check(p.valid && p.geom_id >= 0, "几何拾取填充 geom_id");
        }
    }

    // ---- 生命周期：关闭开关触发下线（on_deactivate + 清空产出）----
    interaction_enabled = false;
    pickWorld(service, renderer.GetPointer(), mesh.vertex_positions_[0]); // 驱动一次 syncState
    check(fake.deactivate_count == 1, "下线时调用 onDeactivate 一次");
    check(!service.hasActiveState(), "开关关闭后无激活交互");

    // ---- syncPending：模拟 GUI 线程 setActive 置位 + notify 到达，驱动激活迁移 ----
    interaction_enabled = true;
    fake.state.needs_refresh = true; // setActive(true)：置位 + notify
    service.syncPending();
    check(fake.activate_count == 2, "syncPending 驱动上线（onActivate 再次调用）");
    interaction_enabled = false;
    fake.state.needs_refresh = true; // setActive(false)：置位 + notify
    service.syncPending();
    check(fake.deactivate_count == 2, "syncPending 驱动下线（onDeactivate 再次调用）");
    check(!service.hasActiveState(), "syncPending 下线后无激活交互");

    // ---- 下线即消费挂起状态：needs_refresh/deferred_op 随 clearSession 失效，不堵死后续 notify ----
    interaction_enabled = true;
    fake.state.needs_refresh = true;
    service.syncPending();
    check(fake.activate_count == 3, "syncPending 再次驱动上线");
    interaction_enabled = false;
    fake.state.needs_refresh = true;      // setActive(false) 置位
    fake.state.deferred_op = [] { };      // 尚未执行的延迟操作
    service.syncPending();                // 下线：clearSession 应消费挂起状态
    check(fake.deactivate_count == 3, "syncPending 再次驱动下线");
    check(!fake.state.needs_refresh, "下线时 needs_refresh 被消费（不堵死后续 notify）");
    check(!fake.state.deferred_op, "下线时 deferred_op 随会话清理");

    spdlog::info("==== InteractionService self-check: {} ====",
        g_failures == 0 ? "ALL PASS" : "HAS FAILURES");

    // geom_index 等局部对象须存活到交互结束
    renderWindow->Render();
    interactor->Start();
    return g_failures;
}
