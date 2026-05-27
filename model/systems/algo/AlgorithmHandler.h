/**
 * @file AlgorithmHandler.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef ALGORITHM_HANDLER_H
#define ALGORITHM_HANDLER_H
#include "ArgType.h"
#include "ComponentOperator.h"

#include <any>
#include <string>
#include <vector>

namespace core {
class ArgObject;
}
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
