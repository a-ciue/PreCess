/**
 * @file InteractionService.h
 * @brief 通用视口交互服务：拾取解析 + 标注绘制 + 事件路由，与具体插件解耦
 */
#ifndef INTERACTION_SERVICE_H
#define INTERACTION_SERVICE_H

#include "InteractiveTypes.h"

#include <functional>
#include <vector>
#include <vtkSmartPointer.h>

class vtkRenderer;
class vtkActor;
class vtkPolyData;
class vtkBillboardTextActor3D;
class SelectManager;
class IMeshIdQuery;

namespace systems::interaction {
struct InteractionState;
}

/**
 * @brief 通用视口交互服务
 *
 * 经 state_provider 获取当前激活的 InteractionState（FeatureSystem::activeInteraction），
 * 将左键/悬停解析为 PickInfo 调用其回调，并按其 annotations 统一绘制标注。
 * 无会话概念：激活状态迁移（功能参数开关驱动）在 pick/hover 入口自动同步。
 * 与选择系统的互斥由 QRenderWindowStyle 按 hasActiveState() 路由；
 * 网格/几何顶点吸附均经 SelectManager 封装接口完成，不直接接触 picker。
 */
class InteractionService {
public:
    InteractionService(vtkRenderer& renderer, vtkRenderer& overlay_renderer,
        SelectManager& select_manager);
    ~InteractionService();

    //! @brief 交互状态提供者（由 QRenderWindow 注入，转发 FeatureSystem::activeInteraction）
    std::function<systems::interaction::InteractionState*()> state_provider;

    //! @brief 注入模型层 id 查询（局部点 id -> 全局点 id 换算，随 setModelQuery 桥接）
    void setMeshIdQuery(const IMeshIdQuery* id_query) { id_query_ = id_query; }

    //! @brief 当前是否存在激活的交互（鼠标路由用）
    bool hasActiveState();

    //! @brief 左键点击：同步激活状态，解析吸附点并调用 on_pick 回调
    void pick(double posx, double posy);
    //! @brief 悬停：同步激活状态，调用 on_hover 回调做动态预览
    void hover(double posx, double posy);
    //! @brief 由 notify 回调驱动（GUI 线程 setActive/requestRefresh 后）：同步激活迁移并拉取标注刷新 VTK actor
    void syncPending();

private:
    //! @brief 同步激活状态：迁移时执行下线（on_deactivate/清标注/还原吸附）与上线（吸附/on_activate/刷新）
    systems::interaction::InteractionState* syncState();
    //! @brief 网格顶点优先、几何顶点兜底的吸附解析；命中时 out.valid 置 true
    bool snapToPickInfo(double posx, double posy, systems::interaction::PickInfo& out);
    //! @brief 拉取标注集并刷新全部 actor
    void refreshAnnotations();
    void clearActors();

    vtkRenderer* renderer_ {};
    vtkRenderer* overlay_renderer_ {};
    SelectManager* select_manager_ {};
    const IMeshIdQuery* id_query_ {};

    systems::interaction::InteractionState* current_ {}; //> 当前已上线的交互状态

    vtkSmartPointer<vtkPolyData> points_poly_;
    vtkSmartPointer<vtkActor> points_actor_;
    vtkSmartPointer<vtkPolyData> lines_poly_;
    vtkSmartPointer<vtkActor> lines_actor_;
    vtkSmartPointer<vtkPolyData> dashed_poly_;
    vtkSmartPointer<vtkActor> dashed_actor_;

    std::vector<vtkSmartPointer<vtkBillboardTextActor3D>> text_pool_;
};

#endif // INTERACTION_SERVICE_H
