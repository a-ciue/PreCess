/**
 * @file TetGenLibHandler.h
 * @author 范成通 1941804585@qq.com
 * @brief TetGen 库模式四面体剖分处理器声明
 * @date 2026-06-24
 */
#pragma once
#include "AlgorithmHandler.h"
#include <any>
#include <optional>
#include <vector>

namespace systems::algo {
/**
 * @brief 基于 TetGen 库接口的四面体剖分
 *
 * 通过 args[0] 的 Selector（Component 类型选择）解析目标 component，覆盖
 * @c AlgorithmHandler::resolveComponentId 按选择器选中的组件直接解析，
 * 不依赖对象树传入的 fallback_component_id。
 */
class TetGenLibHandler : public AlgorithmHandler {
public:
    TetGenLibHandler() = default;
    ~TetGenLibHandler() override = default;

    std::optional<Index> resolveComponentId(
        ModelLayer& model_layer,
        Index fallback_component_id,
        const std::vector<core::ArgObject>& args) const override;
    std::any execute(HandlerContext& context, const std::vector<core::ArgObject>& args) override;
    std::vector<core::ArgType> args_type() const override;
};
}
