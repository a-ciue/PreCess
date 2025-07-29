#pragma once
#include "../../systems/algo/AlgorithmHandler.h"
#include <any>
#include <vector>

namespace systems::algo {
// 适配AlgorithmHandler接口，内部调用CmdExecuteCommand
class CmdExecuteHandler : public AlgorithmHandler {
public:
    CmdExecuteHandler() = default;
    ~CmdExecuteHandler() override = default;
    std::any execute(HandlerContext& context, const std::vector<std::any>& args) override;
    std::vector<ArgType> args_type() const override;
};
// 已废弃，无需实现。
}