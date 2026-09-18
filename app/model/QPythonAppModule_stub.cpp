/**
 * @file QPythonAppModule_stub.cpp
 * @brief python_app 模块注册桩实现：Python 嵌入不可用时的降级载体
 *
 * precess_bindings 目标不存在（Python3/pybind11 不可用、PRECESS_BUILD_PYTHON=OFF
 * 或 wasm 构建）时由 CMake 以本实现编入 modelQml，业务代码无 #ifdef——注册为
 * 空操作，"不可用"是运行时状态。
 */
#include "QPythonAppModule.h"

namespace python_app {

void registerAppModule(QObject* /*owner*/) { }

void shutdownAppModule() { }

}
