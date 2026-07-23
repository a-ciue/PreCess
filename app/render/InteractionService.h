/**
 * @file InteractionService.h
 * @brief 通用视口交互服务：拾取解析 + 标注绘制 + 事件路由，与具体插件解耦
 */
#ifndef INTERACTION_SERVICE_H
#define INTERACTION_SERVICE_H

#include "InteractiveTypes.h"

#include <functional>
#include <string>
#include <vector>
#include <vtkSmartPointer.h>

class vtkRenderer;
class vtkActor;
class vtkPolyData;
class vtkHardwarePicker;
class vtkBillboardTextActor3D;
class MeshActorManagerSelectOp;
class SelectManager;

namespace systems::interaction {
struct InteractionState;
}

/**
 * @brief 通用视口交互服务
 *
 * start 某个 InteractionState 后，将左键/悬停解析为 PickInfo 调用其回调，
 * 并按其 annotations 统一绘制标注（点/实线/虚线/叠加层文本）。
 * 与选择系统的互斥由 QRenderWindowStyle 按 isActive() 路由；
 * 几何顶点吸附经 SelectManager 封装接口完成，不直接接触 picker。
 */
class InteractionService {
public:
    InteractionService(vtkRenderer& renderer, vtkRenderer& overlay_renderer,
        MeshActorManagerSelectOp& mesh_op, SelectManager& select_manager);
    ~InteractionService();

    //! @brief 启动某个交互状态（几何吸附切到顶点模式，on_activate 后刷新标注）
    void start(systems::interaction::InteractionState* state);
    //! @brief 停止交互（on_deactivate、还原几何吸附模式、清空标注与结果）
    void stop();
    bool isActive() const { return state_ != nullptr; }

    //! @brief 左键点击：解析吸附点并调用 on_pick 回调
    void pick(double posx, double posy);
    //! @brief 悬停：调用 on_hover 回调做动态预览
    void hover(double posx, double posy);
    //! @brief 面板"清除"：调用 on_clear 回调并刷新标注与结果
    void clear();

    //! @brief 结果文本变化回调（由 QRenderWindow 设置为发 Qt 信号），渲染线程内触发
    std::function<void(const std::string&)> onResultChanged;

private:
    //! @brief 网格顶点优先、几何顶点兜底的吸附解析；命中时 out.valid 置 true
    bool snapToPickInfo(double posx, double posy, systems::interaction::PickInfo& out);
    //! @brief 拉取标注集并刷新全部 actor
    void refreshAnnotations();
    void emitResult();

    vtkRenderer* renderer_ {};
    vtkRenderer* overlay_renderer_ {};
    MeshActorManagerSelectOp* mesh_op_ {};
    SelectManager* select_manager_ {};

    systems::interaction::InteractionState* state_ {};

    vtkSmartPointer<vtkHardwarePicker> mesh_picker_;

    vtkSmartPointer<vtkPolyData> points_poly_;
    vtkSmartPointer<vtkActor> points_actor_;
    vtkSmartPointer<vtkPolyData> lines_poly_;
    vtkSmartPointer<vtkActor> lines_actor_;
    vtkSmartPointer<vtkPolyData> dashed_poly_;
    vtkSmartPointer<vtkActor> dashed_actor_;

    std::vector<vtkSmartPointer<vtkBillboardTextActor3D>> text_pool_;
};

#endif // INTERACTION_SERVICE_H
