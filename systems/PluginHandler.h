#ifndef SYSTEMS_PLUGIN_HANDLER_H
#define SYSTEMS_PLUGIN_HANDLER_H

#include "PluginBase.h"
#include  <stdexcept>

namespace systems {
/**
 * @brief 插件处理器对象，跨dll动态库从插件中获取的，可据此访问插件中的处理器对象。需要指定处理器类型T进行转型。
 * 
 * @tparam SystemHandler 系统级Handler处理器类型。注意特定插件提供的Handler处理器是该SystemHandler的派生类型，。
 */
template <typename SystemHandler>
class PluginHandler {
public:
	/**
     * @exception std::runtime_error 如果插件无法通过creator函数创建处理器对象
     * @exception std::bad_any_cast 如果插件提供的处理器类型与T不匹配
	 * @param plugin 导入的插件对象
	 */
	PluginHandler(PluginBase& plugin)
        : handler_(std::any_cast<SystemHandler*>(plugin.getHandlerCreatorDestroyer().creator()))
        , destroyer_(plugin.getHandlerCreatorDestroyer().destroyer)
    {
        if (!handler_) {
            throw std::runtime_error("Failed to create handler from plugin");
        }
        // 确保handler_不非空
    }

    ~PluginHandler() noexcept
    {
        destroyer_(handler_);
        handler_ = nullptr; // 确保处理器被销毁
    }
    PluginHandler(const PluginHandler&) = delete; // 禁止拷贝构造
    PluginHandler& operator=(const PluginHandler&) = delete; // 禁止拷贝赋值
    PluginHandler(PluginHandler&&) = delete; // 禁止移动构造
    PluginHandler& operator=(PluginHandler&&) = delete; // 禁止移动赋值

    /**
     * @brief 获取Handler对象引用
     */
    SystemHandler& handler() noexcept
    {
        return *handler_;
    }

private:
    SystemHandler* handler_; //> 存储处理器对象
    const std::function<void(std::any)>& destroyer_; //> 处理器销毁函数
};
}

#endif // SYSTEMS_PLUGIN_HANDLER_H