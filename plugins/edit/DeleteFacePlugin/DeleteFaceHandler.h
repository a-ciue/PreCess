#pragma once
#include "EditHandler.h"
#include <any>
#include <vector>

namespace systems::edit {
/**
 * @brief 删除面的模型编辑处理器Handler，代码实现了删除面的功能
 */
class DeleteFaceHandler : public EditHandler {
public:
    DeleteFaceHandler() = default;
    ~DeleteFaceHandler() override = default;
    std::any execute(ComponentOperator& op, const std::vector<core::ArgObject>& args) override;
    std::vector<core::ArgType> args_type() const override;
};
}