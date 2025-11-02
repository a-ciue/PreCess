/**
 * @file EditHandler.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef EDIT_HANDLER_H
#define EDIT_HANDLER_H
#include "ArgType.h"
#include "ModelData.h"

#include <any>
#include <string>
#include <vector>

namespace core {
class ArgObject;
}

namespace systems::edit {
/**
 * @brief 模型编辑系统的功能接口，继承他来实现具体的编辑功能
 */
class EditHandler {
public:
    virtual ~EditHandler() = default;
    /**i安吉
     * @brief 执行模型编辑功能
     * @param model 待操作的模型数据
     * @param args 模型编辑参数
     * @return 模型编辑结果模型数据
     */
    virtual ModelData execute(ModelData model, const std::vector<core::ArgObject>& args) = 0;
    /**
     * @brief 模型编辑参数类型，交给UI使用
     * @return 返回参数类型列表
     */
    virtual std::vector<core::ArgType> args_type() const = 0;
};
}
#endif // EDIT_HANDLER_H
