#pragma once
#include "EditHandler.h"
#include <any>
#include <vector>

namespace systems::edit {
/**
 * @brief 删除面的模型编辑处理器Handler，代码实现了删除面的功能
 * @note 目标组件由选择集携带的 component_id 决定（面 id 为组件内局部 id），
 *       选择未携带组件身份时以接口提示的 fallback_component_id 兜底
 */
class DeleteFaceHandler : public EditHandler {
public:
    DeleteFaceHandler() = default;
    ~DeleteFaceHandler() override = default;
    std::any execute(ModelLayer& model, Index fallback_component_id, const std::vector<core::ArgObject>& args) override;
    std::vector<core::ArgType> args_type() const override;
};
}