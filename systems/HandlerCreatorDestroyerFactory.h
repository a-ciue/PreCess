#ifndef SYSTEMS_HANDLER_CREATOR_FACTORY
#define SYSTEMS_HANDLER_CREATOR_FACTORY

#include "HandlerCreatorDestroyer.h"
#include <memory>
#include <spdlog/spdlog.h>

namespace systems {
template <typename Handler, typename SystemHandler>
class HandlerCreatorDestroyerFactory {
public:
    static const systems::HandlerCreatorDestroyer& get()
    {
        static const HandlerCreatorDestroyer handler_creator_destroyer = {
            []() {
                SystemHandler* ret {};
                try {
                    auto handler = std::make_unique<Handler>();
                    ret = handler.release(); // 返回指针，避免std::unique_ptr的析构
                } catch (const std::exception& e) {
                    // 跨dll边界不能传递异常，所以这里捕获异常并处理，处理此时返回空指针
                    ret = nullptr;
                }
                return ret;
            },
            [](std::any handler) {
                try {
                    if (handler.has_value()) {
                        delete std::any_cast<SystemHandler*>(handler);
                    }
                } catch (const std::bad_any_cast& e) {
                    // 跨dll边界不能传递异常，只能默许内存泄露不做处理
                    spdlog::error("Failed to cast handler in destroyer: {}", e.what());
                } catch (const std::exception& e) {
                    // 捕获其他异常
                    spdlog::error("Exception in destroyer: {}", e.what());
                }
            }
        };
        return handler_creator_destroyer;
    }
};
}

#endif // SYSTEMS_HANDLER_CREATOR_FACTORY