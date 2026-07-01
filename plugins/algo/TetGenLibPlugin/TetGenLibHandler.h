/**
 * @file TetGenLibHandler.h
 * @author 范成通 1941804585@qq.com
 * @brief TetGen 库模式四面体剖分处理器声明
 * @date 2026-06-24
 */
#pragma once
#include "AlgorithmHandler.h"
#include <any>
#include <vector>

namespace systems::algo {
class TetGenLibHandler : public AlgorithmHandler {
public:
    TetGenLibHandler() = default;
    ~TetGenLibHandler() override = default;

    std::any execute(HandlerContext& context, const std::vector<core::ArgObject>& args) override;
    std::vector<core::ArgType> args_type() const override;
};
}
