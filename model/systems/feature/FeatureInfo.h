/**
 * @file FeatureInfo.h
 * @brief 功能的描述信息与注册声明（菜单、按键）
 */
#ifndef FEATURE_INFO_H
#define FEATURE_INFO_H
#include "ArgType.h"
#include "FeatureNavigation.h"

#include <string>
#include <vector>

namespace systems::feature {
/**
 * @brief 按键绑定：明确声明正式 execute，或定向预览/非模型 onKeyEvent 回调
 */
struct KeyBinding {
    int key { 0 }; //> Qt::Key 键码（int 存储）
    int modifiers { 0 }; //> Qt::KeyboardModifiers 组合（int 存储）
    bool execute { false }; //> true = 明确的 execute 快捷命令；false = 预览/非模型事件回调
};

/**
 * @brief 功能信息：元数据与 setup() 声明的聚合，供 UI 查询
 */
struct FeatureInfo {
    std::string name; //> 功能唯一名称，用作索引
    std::string display_name; //> 功能 UI 展示用名称
    std::string description; //> 功能描述
    bool interactive = false; //> 是否声明视口交互能力（功能经 interaction 上下文订阅交互回调）
    std::vector<core::ArgType> arg_types; //> 功能参数类型列表
    FeatureNavigation navigation; //> setup 声明的统一功能入口
    std::vector<KeyBinding> key_bindings; //> 按键绑定列表
};
}
#endif // FEATURE_INFO_H
