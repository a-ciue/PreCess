/**
 * @file AlgorithmHandler.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef ALGORITHM_HANDLER_H
#define ALGORITHM_HANDLER_H
#include "ArgType.h"
#include "ComponentOperator.h"

#include <any>
#include <optional>
#include <string>
#include <vector>

namespace core {
class ArgObject;
}
class ModelLayer;
namespace systems::io
{
class ModelIOSystemBase;
}

namespace systems::algo {
struct HandlerContext {
    io::ModelIOSystemBase& io_system;
    ComponentOperator& cur_component;
};
/**
 * @brief 算法系统的功能接口，继承他来实现具体的算法功能
 */
class AlgorithmHandler {
public:
    virtual ~AlgorithmHandler() = default;
    /**
     * @brief 在执行前解析算法实际使用的 Component
     *
     * 默认使用对象树传入的 Component；需要根据参数定位目标的插件可覆盖该函数。
     *
     * @param model_layer 模型数据层
     * @param fallback_component_id 对象树当前 Component ID
     * @param args 算法参数
     * @return 实际目标 Component ID；无法确定时返回空
     */
    virtual std::optional<Index> resolveComponentId(
        ModelLayer& model_layer,
        Index fallback_component_id,
        const std::vector<core::ArgObject>& args) const
    {
        (void)model_layer;
        (void)args;
        if (fallback_component_id < 0)
            return std::nullopt;
        return fallback_component_id;
    }
    /**
     * @brief 执行算法功能
     * @param context
     * @param args 算法参数
     * @return 算法结果（可自定义类型）
     */
    virtual std::any execute(HandlerContext& context, const std::vector<core::ArgObject>& args) = 0;
    /**
     * @brief 算法参数类型，交给UI使用
     * @return 返回参数类型列表
     */
    virtual std::vector<core::ArgType> args_type() const = 0;
};
}
#endif // ALGORITHM_HANDLER_H
