/**
 * @file MeasureHandler.h
 * @brief 测量功能处理器声明：距离、角度、半径、长度、面积、体积、包围盒与重心
 * @author 范成通 email 1941804585@qq.com
 */
#pragma once
#include "FeatureHandler.h"
#include "InteractiveTypes.h"
#include <any>
#include <optional>
#include <thread>
#include <vector>

namespace systems::feature {

using systems::interaction::AnnotationBatch;
using systems::interaction::AnnotationLine;
using systems::interaction::PickInfo;

/**
 * @brief 测量功能处理器：尺寸标注（FeatureHandler）+ 交互测量（经 InteractionContext 注册回调）
 *
 * 尺寸标注：功能参数（测量类型 + 选择对象）+ 菜单执行 → 返回结果文本（GUI 线程调用）。
 * 交互测量：两点成线、共端点两线自动标注夹角（渲染线程经 InteractionService 驱动）。
 *
 * @par 线程模型与共享约束（修改前必读）
 * - setup()/execute() 由 **GUI 线程**调用（FeatureSystem 注册与菜单触发）。
 * - onPick/onHover/annotations/clear 由 **渲染线程**调用
 *   （InteractionService 经 dispatch_async 驱动；本类在 activate() 中经 ctx.interaction 注册回调）。
 * - 因此约定：**setup()/execute() 不得读写交互状态成员**
 *   （pending_、has_preview_、preview_、lines_、angles_）；
 *   **交互事件方法不得触碰标注执行路径的状态**（execute 当前是无状态的，新增状态前必须评审）。
 * - 本类**无锁**。UI 层的模式互斥（交互模式隐藏执行按钮、参数模式停止交互）只是当前的安全网，
 *   不是设计依据；确需跨两侧共享状态时，先加锁或经设计评审，不得直接访问。
 *   上述线程模型由 assertExecuteThread()/assertInteractiveThread() 在调试期以 assert 强制。
 */
class MeasureHandler : public FeatureHandler {
public:
    MeasureHandler() = default;
    ~MeasureHandler() override = default;

    //! @brief 声明功能参数（测量类型、选择对象）与菜单项
    void setup(FeatureRegistrar& reg) override;
    //! @brief 激活：经 ctx.interaction 注册交互回调（拾取/悬停/激活/停用/清除）
    void activate(FeatureContext& ctx) override;
    //! @brief 尺寸标注：按参数中的测量类型与选择对象执行，返回结果文本
    std::any execute(FeatureContext& ctx) override;

    //! @brief 交互测量状态查询（测试与面板用）
    int lineCount() const { return static_cast<int>(lines_.size()); }
    bool hasPending() const { return pending_.has_value(); }

private:
    // ---- 交互回调（原 InteractiveHandler 接口方法，现由 activate() 注册到 InteractionContext） ----
    bool onPick(const PickInfo& pick);
    bool onHover(const PickInfo& pick);
    const AnnotationBatch& annotations() const { assertInteractiveThread(); return *annotations_; }
    void clear();

    //! @brief 交互测量线：两个吸附点（PickInfo 已含世界坐标与两套顶点 id）
    struct MeasureLine {
        PickInfo a, b;
    };
    //! @brief 共端点两线的夹角：at 为共点，p/q 为两线各自另一端点
    struct MeasureAngle {
        int line1 = 0; //> lines_ 下标
        int line2 = 0;
        std::array<double, 3> at;
        std::array<double, 3> p;
        std::array<double, 3> q;
        double angle = 0.0;
    };

    static bool samePoint(const PickInfo& a, const PickInfo& b);
    void addLine(const PickInfo& a, const PickInfo& b);
    //! @brief 由当前状态重建标注集
    void refreshAnnotations();

    //! @brief 线程亲和守卫（仅调试期生效）：setup/execute 必须在构造线程（GUI）调用
    void assertExecuteThread() const;
    //! @brief 线程亲和守卫（仅调试期生效）：交互方法必须始终在单一线程上调用
    //! （应用内为渲染线程；测试单线程环境下与构造线程相同也合法）
    void assertInteractiveThread() const;

    std::optional<PickInfo> pending_; //> 已起笔未成线的首点
    bool has_preview_ = false; //> 悬停是否正在预览
    PickInfo preview_ {}; //> 悬停预览吸附点
    // 以下交互状态成员仅允许渲染线程的交互方法访问（见类注释的线程模型约定）
    std::vector<MeasureLine> lines_;
    std::vector<MeasureAngle> angles_;
    // 标注集契约：功能在回调中直接更新 InteractionState.annotations（activate 时绑定），
    // 渲染层事件后拉取绘制；自持成员会导致渲染层永远拉到空标注
    AnnotationBatch* annotations_ { nullptr };

    const std::thread::id gui_thread_id_ = std::this_thread::get_id(); //> 构造线程（GUI 线程）id
    mutable std::thread::id interactive_thread_id_ {}; //> 首个交互调用所在线程 id
};
}
