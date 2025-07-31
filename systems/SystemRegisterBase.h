/**
 * @file SystemRegisterBase.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef SYSTEM_REGISTER_BASE_H
#define SYSTEM_REGISTER_BASE_H
#include <QJsonObject>
#include <any>

namespace systems {
class PluginBase;
/**
 * @brief 系统功能插件的注册器，系统的交互接口，用于注册和注销系统的插件PluginBase。本身不储存状态
 */
class SystemRegisterBase {
public:
    virtual ~SystemRegisterBase() = default;
    /**
     * @brief 注册一个插件。
     * @param meta_data 插件的元信息，包括包含的处理器的类型、名称等信息
     * @param plugin 要注册的插件
     * @return 注册是否成功
     */
    virtual bool registerPlugin(const QJsonObject& meta_data, PluginBase& plugin) = 0;
    /**
     * @brief 注销指定的插件
     * @param meta_data 处理器的元信息，包含处理器的类型、名称等信息
     */
    virtual void unregisterPlugin(const QJsonObject& meta_data) = 0;
};
}
#endif // SYSTEM_REGISTER_BASE_H