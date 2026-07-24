/**
 * @file ScalarAttributeDisplayResult.h
 * @brief 标量属性显示结果
 */
#ifndef SCALAR_ATTRIBUTE_DISPLAY_RESULT_H
#define SCALAR_ATTRIBUTE_DISPLAY_RESULT_H

#include <string>

namespace systems::feature {

/**
 * @brief 功能执行后返回的标量属性显示请求
 *
 * 功能只返回结果文本和模型属性名，由 app 层决定如何显示，
 * 避免功能插件直接依赖渲染模块。
 */
struct ScalarAttributeDisplayResult {
    std::string message;
    std::string attribute_name;
};

}
#endif // SCALAR_ATTRIBUTE_DISPLAY_RESULT_H
