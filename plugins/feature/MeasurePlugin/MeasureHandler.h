/**
 * @file MeasureHandler.h
 * @brief 测量处理器声明：交互式两点成线测距与共端点夹角标注
 * @author 范成通 email 1941804585@qq.com
 */
#pragma once
#include "EventBus.h"
#include "FeatureHandler.h"
#include "InteractiveTypes.h"
#include <array>
#include <optional>
#include <vector>

namespace systems::feature {

using systems::interaction::AnnotationBatch;
using systems::interaction::AnnotationLine;
using systems::interaction::PickInfo;

/**
 * @brief 测量处理器：交互式两点成线测距与共端点两线夹角标注
 *
 * 纯视口交互功能："清除"按钮参数经 ParameterChangedEvent 触发清空，
 * 交互回调（拾取/悬停/激活/停用）由渲染线程经 InteractionService 驱动，
 * 全部状态成员仅被交互回调访问。
 */
class MeasureHandler : public FeatureHandler {
public:
    MeasureHandler() = default;
    ~MeasureHandler() override = default;

    //! @brief 声明"清除"按钮参数与菜单项
    void setup(FeatureRegistrar& reg) override;
    //! @brief 激活：经 ctx.interaction 注册交互回调（拾取/悬停/激活/停用），订阅参数变更事件
    void activate(FeatureContext& ctx) override;

    //! @brief 交互测量状态查询（测试与面板用）
    int lineCount() const { return static_cast<int>(lines_.size()); }
    bool hasPending() const { return pending_.has_value(); }

private:
    // ---- 交互回调（由 activate() 注册到 InteractionContext，渲染线程驱动） ----
    bool onPick(const PickInfo& pick);
    bool onHover(const PickInfo& pick);
    void clear();

    //! @brief 交互测量线：两个吸附点（PickInfo 已含世界坐标与两套顶点 id）
    struct MeasureLine {
        PickInfo a, b;
    };
    //! @brief 共端点两线的夹角：at 为共点，p/q 为两线各自另一端点
    struct MeasureAngle {
        std::array<double, 3> at;
        std::array<double, 3> p;
        std::array<double, 3> q;
        double angle = 0.0;
    };

    void addLine(const PickInfo& a, const PickInfo& b);
    //! @brief 由当前状态重建标注集
    void refreshAnnotations();

    std::optional<PickInfo> pending_; //> 已起笔未成线的首点
    std::optional<PickInfo> preview_; //> 悬停预览吸附点
    std::vector<MeasureLine> lines_;
    std::vector<MeasureAngle> angles_;
    // 标注集契约：功能在回调中直接更新 InteractionState.annotations（activate 时绑定），
    // 渲染层事件后拉取绘制；自持成员会导致渲染层永远拉到空标注
    AnnotationBatch* annotations_ { nullptr };
    core::EventBus::Subscription param_sub_; //> ParameterChangedEvent 订阅句柄（析构自动退订）
};
}
