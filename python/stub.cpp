/**
 * @file stub.cpp
 * @brief python::Runtime 桩实现：Python 嵌入不可用时的降级载体
 *
 * precess_runtime 目标恒存在（Python3/pybind11 不可用、PRECESS_BUILD_PYTHON=OFF
 * 或 wasm 构建时以本实现编译），GUI/CLI 宿主无需条件编译——"不可用"是运行时
 * 状态：available 恒 false，错误信息说明原因。
 */
#include "python/runtime.h"

#include <utility>

namespace python {

struct Runtime::State {
    std::string last_error; //> 固定的不可用原因（构造即置位，不重试）
};

Runtime::Runtime(session::Session* /*session*/, Config /*config*/)
    : state_(std::make_unique<State>())
{
    state_->last_error
        = "编译时未启用内嵌 Python 运行环境（需要 Python3 解释器、开发库与 pybind11；"
          "wasm 构建不支持，或经 PRECESS_BUILD_PYTHON=OFF 显式关闭）";
}

Runtime::~Runtime() = default;

bool Runtime::isAvailable() const
{
    return false;
}

const std::string& Runtime::lastError() const
{
    return state_->last_error;
}

void Runtime::initialize() { }

Runtime::ExecutionResult Runtime::execute(const std::string& /*source*/)
{
    ExecutionResult result;
    result.error = state_->last_error;
    return result;
}

std::string Runtime::version() const
{
    return std::string();
}

}
