/**
 * @file SystemHandlerPtr.h
 * @author 张家僮 2025~
 */
#ifndef SYSTEM_HANDLER_PTR_H
#define SYSTEM_HANDLER_PTR_H
#include <memory>

namespace systems {
/**
 * @brief SystemHandlerPtr的智能指针析构器，支持自定义销毁函数
 * @sa systems::SystemHandlerPtr
 * @tparam SystemHandler 系统处理器基类类型
 */
template <typename SystemHandler>
struct SystemHandlerDestroyer {
    void operator()(SystemHandler* handler) const noexcept {
        if (!destroyer) {
            delete handler;
        } else {
            destroyer(handler);
        }
    }

    void (*destroyer)(SystemHandler*) {}; //> 自定义的销毁函数指针，为空时使用默认析构
};

/**
 * @brief 系统处理器智能指针，支持自定义销毁函数
 * @sa systems::SystemHandlerDestroyer
 */
template <typename SystemHandler>
using SystemHandlerPtr = std::unique_ptr<SystemHandler, SystemHandlerDestroyer<SystemHandler>>;
}

#endif // SYSTEM_HANDLER_PTR_H
