/**
 * @file QmlRegistration.h
 * @brief 停靠组件对外入口：QML 类型注册与环境初始化
 */

#pragma once

#include <QtGlobal>

QT_BEGIN_NAMESPACE
class QQmlEngine;
QT_END_NAMESPACE

namespace dock {

/**
 * @brief 初始化停靠组件
 *
 * 注册 `PreCess.Docking` QML 类型（DockHost/DockPanel/Enums），
 * 注入 QML 引擎并安装全局鼠标过滤器。应在加载 QML 之前调用。
 */
void init(QQmlEngine* engine);

namespace ui {
    //! @brief 注册 QML 类型（可重复调用，Qt 会忽略重复注册）
    void registerTypes();
}
}
