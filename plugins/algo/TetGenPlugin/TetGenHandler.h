#pragma once
#include "AlgorithmHandler.h"
#include <any>
#include <vector>

namespace systems::algo {
// 适配AlgorithmHandler接口，内部调用TetGenCommand
class TetGenHandler : public AlgorithmHandler {
public:
    TetGenHandler() = default;
    ~TetGenHandler() override = default;
    std::any execute(HandlerContext& context, const std::vector<std::any>& args) override;
    std::vector<ArgType> args_type() const override;
};
}