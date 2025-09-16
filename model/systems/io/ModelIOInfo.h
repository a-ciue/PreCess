#ifndef MODEL_IO_INFO_H
#define MODEL_IO_INFO_H
#include "ArgType.h"
#include <string>
#include <vector>
namespace systems::io {
/**
 * @brief 模型层文件读写支持的文件类型信息
 */
struct ModelIOInfo {
    std::string name; //> 文件类型唯一名称，用作索引
    std::string description; //> 文件类型描述
    std::vector<std::string> extensions; //> 文件类型对应拓展名列表，如 ["txt", "obj"]。为空时表示支持所有拓展名
    std::vector<ArgType> read_arg_types; //> 读操作参数类型列表
    std::vector<ArgType> write_arg_types; //> 写操作参数类型列表
};
}

#endif // MODEL_IO_INFO_H