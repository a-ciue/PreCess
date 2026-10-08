#ifndef ALGORITHM_INFO_H
#define ALGORITHM_INFO_H
#include "ArgType.h"
#include <map>
#include <string>
#include <vector>
namespace systems::algo {
/**
 * @brief 插件声明的算法导航信息；类别使用稳定标识，空或未知类别由界面归入其他算法。
 */
struct AlgorithmNavigation {
    std::vector<std::string> categories; //> triangle / quadrilateral / tetrahedron / hexahedron，可声明多类
    std::string group; //> 分类页内分组，空时使用默认分组
    std::string icon; //> 插件提供的 qrc 图标路径
    int order { 0 }; //> 页内排序，数值越小越靠前
    std::string label; //> 面向用户的算法名称，空时沿用 display_name
    std::map<std::string, std::map<std::string, std::string>> category_defaults; //> 类别 -> 参数名 -> 默认值
};
struct AlgorithmInfo {
    std::string name; //> 算法唯一名称，用作索引
    std::string display_name; //> 算法UI展示用名称
    std::string description; //> 算法描述
    std::vector<core::ArgType> arg_types; //> 算法参数类型列表
    AlgorithmNavigation navigation; //> 导航声明
};
}

#endif // ALGORITHM_INFO_H
