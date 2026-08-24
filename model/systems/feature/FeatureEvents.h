/**
 * @file FeatureEvents.h
 * @brief 功能系统在事件总线上传递的事件类型
 */
#ifndef FEATURE_EVENTS_H
#define FEATURE_EVENTS_H
#include "ArgObject.h"
#include "Core.h"

#include <cstddef>
#include <string>
#include <utility>

namespace systems::feature {
/**
 * @brief 键盘按键事件
 *
 * 键码与修饰键以 int 存储（取值为 Qt::Key / Qt::KeyboardModifiers），
 * 避免 model 层依赖 QtGui。
 */
struct KeyEvent {
    int key { 0 }; //> Qt::Key 键码
    int modifiers { 0 }; //> Qt::KeyboardModifiers 组合
    bool pressed { true }; //> true 为按下，false 为释放
};

/**
 * @brief 功能参数变更事件，FeatureSystem::setParameter 成功后发布
 */
struct ParameterChangedEvent {
    ParameterChangedEvent(std::string feature_name, std::size_t index, core::ArgObject new_value)
        : feature(std::move(feature_name))
        , param_index(index)
        , value(std::move(new_value))
    {
    }

    std::string feature; //> 功能唯一名称
    std::size_t param_index { 0 }; //> 参数在参数列表中的下标
    core::ArgObject value; //> 新参数值
};

/**
 * @brief 请求界面渲染指定的标量属性（界面在活动操作切换时自行取消渲染，无需空名事件）
 */
struct ScalarAttributeDisplayRequestedEvent {
    std::string attribute_name; //> 模型中的标量属性名
    Index component_id { -1 }; //> 属性所属的 Component ID
};

/**
 * @brief 模型变更事件，由 app 层桥接 ModelObserver 通知后发布
 */
struct ModelEvent {
    enum class Kind {
        ModelAdded,
        ModelRemoved,
        ModelChanged,
        ComponentChanged,
        ComponentRemoved,
        ModelNameChanged,
    };

    Kind kind;
    Index model_id { -1 };
    Index component_id { -1 };
};
}
#endif // FEATURE_EVENTS_H
