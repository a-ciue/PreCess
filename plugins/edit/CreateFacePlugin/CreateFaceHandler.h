#pragma once
#include "EditHandler.h"
#include <any>
#include <vector>

namespace systems::edit {
/**
 * @brief 创建面的模型编辑处理器Handler，代码实现了创建面的功能
 * @note 目标组件由选择集的全局点 id 经 pointIdMap 反查决定，不依赖对象树选中组件
 */
class CreateFaceHandler : public EditHandler {
public:
    CreateFaceHandler() = default;
    ~CreateFaceHandler() override = default;
    std::any execute(ModelLayer& model, Index fallback_component_id, const std::vector<core::ArgObject>& args) override;
    std::vector<core::ArgType> args_type() const override;
};
}