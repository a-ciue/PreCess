#ifndef ALGORITHM_INFO_H
#define ALGORITHM_INFO_H
#include <string>
struct AlgorithmInfo {
    std::string name; //> 算法唯一名称，用作索引
    std::string display_name; //> 算法UI展示用名称
	std::string description;  //> 算法描述
    std::vector<ArgType> arg_types; //> 算法参数类型列表
};

#endif // ALGORITHM_INFO_H