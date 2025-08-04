/**
 * @file AlgorithmHandler.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef ALGORITHM_HANDLER_H
#define ALGORITHM_HANDLER_H
#include "../../commands/ArgType.h"

#include <any>
#include <string>
#include <vector>

namespace systems::io
{
	class ModelIOSystemBase;
}

class ModelOperatorBase;

namespace systems::algo {
struct HandlerContext {
    io::ModelIOSystemBase& io_system;
    ModelOperatorBase& cur_model;
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
    virtual std::any execute(HandlerContext& context, const std::vector<std::any>& args) = 0;
    /**
     * @brief 算法参数类型，交给UI使用
     * @return 返回参数类型列表
     */
    virtual std::vector<ArgType> args_type() const = 0;
};
}
#endif // ALGORITHM_HANDLER_H
