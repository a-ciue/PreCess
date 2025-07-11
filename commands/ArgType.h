/**
 * @file ArgType.h
 * @brief 用于模型层描述功能参数类型
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef ARG_TYPE_H
#define ARG_TYPE_H
#include <string>

/**
 * @brief 参数类型枚举
 */
#include "ArgTypeEnum.h"

/**
 * @brief 描述某个需要的参数类型，UI根据该类型来生成对应的参数控件
 */
struct ArgType {
    ArgTypeEnum type; // 参数类型
    std::string name; // 参数名称
    std::string content; // 内容（如默认值或其他选项信息），具体形式由参数控件定义
    std::string desc; // 参数描述
};

#endif // !ARG_TYPE_H