/**
 * @file PluginBase.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef PLUGIN_BASE_H
#define PLUGIN_BASE_H
#include "HandlerCreatorDestroyer.h"
#include <any>
#include <functional>

namespace systems {
/**
 * @brief 插件对象基类，定义与插件交互的接口。
 *
 * 事实上取代了传统插件交互的extern C函数的接口，以面向对象的方式提供插件功能。
 */
class PluginBase {
public:
    template<typename T>
    friend class PluginHandler; // 允许PluginHandler访问私有成员

    virtual ~PluginBase() = default;

private:
    /**
     * @brief 获取处理器创建和销毁函数对。
     *
     * 返回一个包含创建和销毁处理器的函数对，创建函数返回一个any类型的处理器指针，销毁函数接受一个any类型的处理器指针。
     * 这允许插件在运行时动态创建和销毁处理器对象。
     * 
     * 由于跨dll传递，所以传递的函数对象引用。
     * 
     * noexcept修饰符表示该函数不会抛出异常，因为跨DLL边界传递异常是不可行的，且实现中只是构造两个lambda函数。
     */
    virtual const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept = 0;
};
}

#ifdef Q_MOC_RUN
Q_DECLARE_INTERFACE(systems::PluginBase, "com.PreCess.systems.PluginBase/1.0")
#endif

#endif // PLUGIN_BASE_H
