/**
 * @file HandlerCreatorDestroyer.h
 * @author 张家僮 2025~
 */
#ifndef SYSTEMS_HANDLER_CREATOR_DESTROYER
#define SYSTEMS_HANDLER_CREATOR_DESTROYER
#include <any>
#include <functional>

namespace systems {
template <typename Handler, typename SystemHandler>
class HandlerCreatorDestroyerFactory;

/**
 * @brief 处理器的创建与删除函数，用于构造与析构插件来的处理器
 *
 * 因为需要保证“从哪个dll new出来的在哪里delete”，
 */
struct HandlerCreatorDestroyer {
    template <typename SystemHandler>
    HandlerCreatorDestroyer(SystemHandler* (*creator_func)(), void (*destroyer_func)(SystemHandler*))
        : creator(creator_func)
        , destroyer(destroyer_func)
    {
    }

    /**
     * @brief 尝试获取插件包裹的Handler具体类型的创建与销毁函数对，利用模板参数尝试转型至基类SystemHandler。
     * @tparam SystemHandler 系统的处理器基类类型
     * @return 返回派生类对象指针存储在SystemHandler基类指针的，创建与销毁函数对
     */
    template <typename SystemHandler>
    auto get() const
    {
        struct result {
            SystemHandler* (*creator)();
            void (*destroyer)(SystemHandler*);
        };

        // cast是在主程序exe做的
        auto creator_p = std::any_cast<SystemHandler* (*)()>(&this->creator);
        auto destroyer_p = std::any_cast<void (*)(SystemHandler*)>(&this->destroyer);
        if (creator_p && destroyer_p) {
            return result { *creator_p, *destroyer_p };
        }
        return result { nullptr, nullptr };
    }

private:
    std::any creator; //> 用于创建处理器对象。
    std::any destroyer; //> 用于销毁从creator获取的处理器对象。
};
}

#endif // SYSTEMS_HANDLER_CREATOR_DESTROYER