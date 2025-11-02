/**
 * @file PluginBase.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef PLUGIN_BASE_H
#define PLUGIN_BASE_H
#include "HandlerCreatorDestroyer.h"
#include "SystemHandlerPtr.h"
#include <any>

namespace systems {
/**
 * @brief 插件对象基类，定义与插件交互的接口。
 *
 * 事实上取代了传统插件交互的extern C函数的接口，以面向对象的方式提供插件功能。
 */
class PluginBase {
public:
    /**
     * @brief 创建插件包裹的Handler派生类型对象，以智能指针管理生命周期。
     * @sa systems::SystemHandlerPtr
     * @sa systems::SystemHandlerDestroyer
     * @tparam SystemHandler 系统的处理器基类类型
     * @return Handler派生对象存储在SystemHandler基类指针，使用各自自定义的销毁函数
     */
    template<typename SystemHandler>
    SystemHandlerPtr<SystemHandler> makeHandler()
    {
        // 调用虚函数getHandlerCreatorDestroyer时跨越dll边界获取std::any包装的函数指针
        auto&& [handlerCreator, handlerDestroyer] = getHandlerCreatorDestroyer().get<SystemHandler>();
        if (handlerCreator && handlerDestroyer) {
            auto handler = handlerCreator();
            SystemHandlerDestroyer<SystemHandler> destroyer { handlerDestroyer };
            return SystemHandlerPtr<SystemHandler>(handler, destroyer);
        }
        return {};
    }

    virtual ~PluginBase() = default;

private:
    /**
     * @brief 获取处理器创建和销毁函数对。
     *
     * @note 因为虚函数在各自插件dll内实现，所以在主程序调用时会跨越dll边界，所以需要noexcept修饰符表示该函数不会抛出异常
     * @note 因为这个对象是静态对象，生命周期与程序相同，故返回引用以避免不必要的拷贝开销。
     * @return 返回一个std::any表示的创建和销毁处理器的函数对，这允许插件在运行时动态创建和销毁处理器对象。
     */
    virtual const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept = 0;
};
}

#ifdef Q_MOC_RUN
Q_DECLARE_INTERFACE(systems::PluginBase, "com.PreCess.systems.PluginBase/1.0")
#endif

#endif // PLUGIN_BASE_H
