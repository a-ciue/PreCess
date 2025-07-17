/**
 * @file SystemRegisterBase.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef SYSTEM_REGISTER_BASE_H
#define SYSTEM_REGISTER_BASE_H
#include <QJsonObject>
#include <any>

namespace systems {
/**
 * @brief 系统功能的注册器，系统的交互接口，用于注册和注销系统处理器Handler。本身不储存状态
 */
class SystemRegisterBase {
public:
    virtual ~SystemRegisterBase() = default;
	/**
	 * @brief 注册一个处理器对象。
     * @param meta_data 处理器的元信息，包含处理器的类型、名称等信息
	 * @param handler 要注册的插件对象
     * @return 注册是否成功
	 */
	virtual bool registerHandler(const QJsonObject& meta_data, std::any handler) = 0;
	/**
	 * @brief 注销指定的处理器
	 * @param meta_data 处理器的元信息，包含处理器的类型、名称等信息
	 */
	virtual void unregisterHandler(const QJsonObject& meta_data) = 0;
};
}
#endif // SYSTEM_REGISTER_BASE_H