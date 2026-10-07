/** @file ShadowComponent.h
 * @brief 算法与功能共用的单组件影子副本；不负责调度和 undo 边界。
 */
#pragma once
#include "ModelLayer.h"
#include "ModelObserver.h"
namespace systems::job {
class ShadowComponent final : private ModelObserver {
public:
    explicit ShadowComponent(const ComponentOperator& source);
    ShadowComponent(const ShadowComponent&) = delete;
    ShadowComponent& operator=(const ShadowComponent&) = delete;
    //! @brief worker 的唯一目标写面。
    ComponentOperator target();
    Index componentId() const { return shadow_component_id_; }
    //! @brief 计算结束后消费标脏通知；失败或取消时无需调用。
    void finishCompute();
    //! @brief GUI 在外层操作授权与记录边界内应用，未改动时空转。
    void apply(ModelLayer& real);

private:
    void notifyComponentChanged(Index id) override { dirty_ = dirty_ || id == shadow_component_id_; }
    void notifyModelChanged(Index) override { }
    void notifyModelAdded(Index) override { }
    void notifyModelRemoved(Index) override { }
    void notifyComponentRemoved(Index) override { }
    void notifyModelNameChanged(Index, const std::string&) override { }
    void notifyGeometryLoadFailed(const std::string&) override { }
    ModelLayer shadow_;
    Index real_component_id_;
    Index shadow_component_id_ { -1 };
    bool dirty_ { false };
};
}
