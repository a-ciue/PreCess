#ifndef EDIT_INFO_H
#define EDIT_INFO_H
#include "ArgType.h"
#include <string>
#include <vector>
namespace systems::edit {
/**
 * @brief 某个模型编辑操作的描述
 */
struct EditInfo {
    std::string name; //> 模型编辑操作唯一名称，用作索引
    std::string display_name; //> 模型编辑操作UI展示用名称
    std::string description; //> 模型编辑操作描述
    std::vector<core::ArgType> arg_types; //> 模型编辑操作参数类型列表
};
}

#endif // EDIT_INFO_H