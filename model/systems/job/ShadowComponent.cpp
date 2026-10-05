/** @file ShadowComponent.cpp
 * @brief 单组件影子的复制、标脏检测与 gid 回写。
 */
#include "ShadowComponent.h"
#include <algorithm>
#include <stdexcept>
namespace systems::job {
namespace {
    void stripGlobalIds(ComponentData& snapshot)
    {
        // 各层发号水位独立；复制进另一层时重新分配身份。
        std::fill(snapshot.point_global_ids_.begin(), snapshot.point_global_ids_.end(), -1);
        snapshot.mesh_adjacency.stripEdgeGlobalIds();
    }
}
ShadowComponent::ShadowComponent(const ComponentOperator& source)
    : shadow_(this)
    , real_component_id_(source.componentId())
{
    shadow_.setOffthreadWritesAllowed(true);
    auto snapshot = source.takeSnapshot();
    stripGlobalIds(*snapshot);
    ComponentDatas mirror;
    mirror.push_back(std::move(snapshot));
    const auto id = shadow_.addModel("__component_shadow__", std::move(mirror));
    shadow_component_id_ = shadow_.modelById(id)->componentIds().front();
}
ComponentOperator ShadowComponent::target()
{
    auto op = shadow_.getComponentOperator(shadow_component_id_);
    if (!op)
        throw std::runtime_error("shadow component missing");
    return *op;
}
void ShadowComponent::finishCompute() { shadow_.flushNotifications(); }
void ShadowComponent::apply(ModelLayer& real)
{
    if (!dirty_)
        return;
    auto op = real.getComponentOperator(real_component_id_);
    if (!op)
        throw std::runtime_error("occupied shadow target disappeared");
    auto computed = target().takeSnapshot();
    stripGlobalIds(*computed);
    op->restoreSnapshot(*computed);
}
}
