/**
 * @file ArgType.h
 * @brief 用于模型层描述功能参数类型
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef ARG_TYPE_H
#define ARG_TYPE_H
#include <string>

// 参数类型枚举
#include "private_ArgTypeEnum.h"

/**
 * @brief 描述算法需要的某个参数的类型，UI根据该类型来生成对应的参数控件
 * 
 * TODO: 参数类型由参数数据类型（int, double, select, choice等），而一种参数类型也可能能用多种控件来实现（如int可以用文本框或滑动条，choice能用dropdown、按钮等），控件类型尽量由UI定义不由集成者定义。即插件开发者只需要做到描述对某个参数的需求，UI根据参数需求来生成对应的控件。可能用std::variant+visit表示控件类型会更好（？）做这个之前学习一下开源软件的思路（blender、unreal engine等）
 */
struct ArgType {
    ArgTypeEnum type; // 参数类型
    std::string name; // 参数名称
    std::string content; // 内容（如默认值或其他选项信息），具体形式由参数控件定义
    std::string desc; // 参数描述
};

#endif // !ARG_TYPE_H