#pragma once
#include "AlgorithmHandler.h"
#include <any>
#include <vector>

namespace systems::algo {
// 适配AlgorithmHandler接口，内部调用CmdExecuteCommand
class CmdExecuteHandler : public AlgorithmHandler {
public:
    CmdExecuteHandler() = default;
    ~CmdExecuteHandler() override = default;
    std::any execute(HandlerContext& context, const std::vector<core::ArgObject>& args) override;
    std::vector<core::ArgType> args_type() const override;
};
}