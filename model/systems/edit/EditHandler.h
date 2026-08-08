/**
 * @file EditHandler.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef EDIT_HANDLER_H
#define EDIT_HANDLER_H
#include "ArgType.h"
#include "Core.h"

#include <any>
#include <string>
#include <vector>

class ModelLayer;

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
    /**
     * @brief 执行模型编辑功能
     * @param model 模型层入口（目标组件经 ModelLayer::getComponentOperator 获取）
     * @param fallback_component_id 对象树当前组件 ID，只视作一种提示（可为 -1）
     * @param args 模型编辑参数
     * @return 模型编辑结果模型数据
     * @note 不要强制要求用户执行前在对象树中选中 component：目标组件应优先由选择器参数
     *       解析（`Selection` 的全局点 id 经 `pointIdMap` 反查，面/边类局部 id 选择携带
     *       `component_id`），fallback_component_id 仅在选择未携带组件身份时兜底。
     *       示例见 `plugins/edit/CreateFacePlugin/`、`plugins/edit/DeleteFacePlugin/`。
     */
    virtual std::any execute(ModelLayer& model, Index fallback_component_id, const std::vector<core::ArgObject>& args) = 0;
    /**
     * @brief 模型编辑参数类型，交给UI使用
     * @return 返回参数类型列表
     */
    virtual std::vector<core::ArgType> args_type() const = 0;
};
}
#endif // EDIT_HANDLER_H
