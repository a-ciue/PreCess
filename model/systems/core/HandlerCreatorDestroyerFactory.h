/**
 * @file HandlerCreatorDestroyerFactory.h
 * @author 张家僮 2025~
 */
#ifndef SYSTEMS_HANDLER_CREATOR_FACTORY
#define SYSTEMS_HANDLER_CREATOR_FACTORY

#include "HandlerCreatorDestroyer.h"
#include <memory>
#include <spdlog/spdlog.h>

namespace systems {
template <typename Handler, typename SystemHandler>
class HandlerCreatorDestroyerFactory {
    static_assert(std::is_base_of_v<SystemHandler, Handler>);

public:
    /**
     * @brief 获取处理器创建与销毁函数对的静态实例。
     *
     * 该函数返回一个静态的HandlerCreatorDestroyer实例，包含用于创建和销毁指定Handler类型的处理器对象的函数。
     *
     * @return 静态的HandlerCreatorDestroyer实例引用。
     */
    static const HandlerCreatorDestroyer& get()
    {
        static const HandlerCreatorDestroyer handler_creator_destroyer(create, destroy);
        return handler_creator_destroyer;
    }

private:
    /**
     * @brief Handler对象的创建函数。
     * @return 存储在SystemHandler基类指针的Handler派生类对象
     */
    static SystemHandler* create()
    try {
        auto handler = std::make_unique<Handler>();
        return handler.release(); // 返回指针，避免std::unique_ptr的析构
    } catch (const std::exception& e) {
        // 跨dll边界不能传递异常，所以这里捕获异常并处理，处理此时返回空指针
        spdlog::error("Exception in creator: {}", e.what());
        return nullptr;
    }
    /**
     * @brief Handler对象的销毁函数。
     * @param handler 待销毁的Handler对象指针，存储在SystemHandler基类指针中
     */
    static void destroy(SystemHandler* handler)
    try {
        delete handler;
    } catch (const std::exception& e) {
        spdlog::error("Exception in destroyer: {}", e.what());
    }
};
}

#endif // SYSTEMS_HANDLER_CREATOR_FACTORY