#pragma once
#include "EditHandler.h"
#include <any>
#include <vector>

namespace systems::edit {
/**
 * @brief 创建面的模型编辑处理器Handler，代码实现了创建面的功能
 */
class CreateFaceHandler : public EditHandler {
public:
    CreateFaceHandler() = default;
    ~CreateFaceHandler() override = default;
    std::any execute(ComponentOperator& op, const std::vector<core::ArgObject>& args) override;
    std::vector<core::ArgType> args_type() const override;
};
}