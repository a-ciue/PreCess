#ifndef ARG_OBJECT_H
#define ARG_OBJECT_H
#include "ArgType.h"
#include <any>

namespace core {
/**
 * @brief 实例化的参数类型，存储参数对象的容器
 */
class ArgObject {
public:
    /**
     * @brief 构造函数，存储参数对象
     * @tparam T 参数类型
     * @param value 待存储的参数对象
     */
    template <ArgTypeEnum T>
    static ArgObject create(ArgTypeT<T> value)
    {
        return std::any(std::move(value));
    }

    /**
     * @brief 获取存储的参数对象
     * @tparam T 参数类型枚举
     * @return 对应的对象指针，失败则为空
     */
    template <ArgTypeEnum T>
    ArgTypeT<T>* get()
    {
        return std::any_cast<ArgTypeT<T>>(&object_);
    }
    template <ArgTypeEnum T>
    const ArgTypeT<T>* get() const
    {
        return const_cast<ArgObject*>(this)->get<T>();
    }

private:
    ArgObject(std::any object)
        : object_(std::move(object))
    {
    }
    std::any object_; //> 类型擦除的参数对象
};
}

#endif