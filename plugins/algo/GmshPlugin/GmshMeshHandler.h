#pragma once
#include "AlgorithmHandler.h"
#include "GmshIncrementalMeshState.h"
#include <any>
#include <unordered_map>
#include <vector>

class ModelLayer;

namespace systems::algo {
class GmshMeshHandler : public AlgorithmHandler {
public:
    GmshMeshHandler() = default;
    ~GmshMeshHandler() override = default;

    /**
     * @brief 根据选中几何面的全局 ID 解析所属 Component
     */
    std::optional<Index> resolveComponentId(
        ModelLayer& model_layer,
        Index fallback_component_id,
        const std::vector<core::ArgObject>& args) const override;
    std::any execute(HandlerContext& context, const std::vector<core::ArgObject>& args) override;
    std::vector<core::ArgType> args_type() const override;

private:
    // 删除已经不存在的 component 对应缓存，避免插件长期运行时残留无效状态。
    void removeExpiredStates(const ModelLayer& model_layer);

    std::unordered_map<Index, GmshIncrementalMeshState> component_states_;
};
} // namespace systems::algo
