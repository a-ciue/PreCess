/**
 * @file AlgorithmHandler.h
 * @author (your name)
 */
#ifndef ALGORITHM_HANDLER_H
#define ALGORITHM_HANDLER_H
#include "../../commands/ArgType.h"
#include "ModelOperator.h"
#include "../io/ModelIOSystem.h"

#include <any>
#include <string>
#include <vector>

namespace systems::algo {
using std::any;
using std::string;
using std::vector;

struct HandlerContext {
    io::ModelIOSystem& io_system;
    ModelOperator& cur_model;
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
    virtual any execute(HandlerContext& context, const vector<any>& args) = 0;
    /**
     * @brief 算法参数类型，交给UI使用
     * @return 返回参数类型列表
     */
    virtual vector<ArgType> args_type() const = 0;
};
}
#endif // ALGORITHM_HANDLER_H
