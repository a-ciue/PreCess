#pragma once
#include "AlgorithmHandler.h"
#include <any>
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

};
} // namespace systems::algo
