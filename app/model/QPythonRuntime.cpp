/**
 * @file QPythonRuntime.cpp
 * @brief QPythonRuntime 实现：python::Runtime 之上的 QObject/QML 薄壳
 *
 * 解释器宿主逻辑在 python/（runtime.cpp 真实现 / stub.cpp 桩，precess_runtime
 * 目标恒存在、无需条件编译）；本文件只做线程断言、字符串编解码、日志与信号
 * 桥接。"不可用"是运行时状态：桩实现 available 恒 false、错误信息说明原因。
 */
#include "QPythonRuntime.h"

#include "python/runtime.h"

#include <QCoreApplication>
#include <QThread>
#include <spdlog/spdlog.h>

#include <filesystem>

// 宿主配置宏由 app/model 仅在真实模式（precess 模块存在）下定义；桩模式下
// Runtime 忽略 config，空值兜底即可
#ifndef PRECESS_PYTHON_HOME
#define PRECESS_PYTHON_HOME ""
#endif
#ifndef PRECESS_PYTHON_MODULE_DIR
#define PRECESS_PYTHON_MODULE_DIR ""
#endif

namespace {

//> 宿主配置：标准库根取 CMake 期绑定的解释器目录（正斜杠，跨机器为构建机
//> 路径，安装分发场景见后续打包阶段）；precess 扩展模块目录候选依次为
//> 部署形态 <exe_dir>/python 与构建树 precess 目标输出目录
python::Runtime::Config makeRuntimeConfig()
{
    python::Runtime::Config config;
    const QString exe_dir = QCoreApplication::applicationDirPath();
    config.python_home = PRECESS_PYTHON_HOME;
    config.module_dirs = {
        std::filesystem::path((exe_dir + QStringLiteral("/python")).toStdString()),
        std::filesystem::path(PRECESS_PYTHON_MODULE_DIR),
    };
    return config;
}

} // namespace

QPythonRuntime::QPythonRuntime(session::Session* session, QObject* parent)
    : QObject(parent)
    , runtime_(std::make_unique<python::Runtime>(session, makeRuntimeConfig()))
{
}

QPythonRuntime::~QPythonRuntime() = default;

bool QPythonRuntime::isAvailable() const
{
    return runtime_->isAvailable();
}

QString QPythonRuntime::lastError() const
{
    return QString::fromStdString(runtime_->lastError());
}

void QPythonRuntime::initialize()
{
    Q_ASSERT(QThread::currentThread() == thread()); // Python ≡ GUI 线程
    if (runtime_->isAvailable() || !runtime_->lastError().empty())
        return;
    runtime_->initialize();
    if (runtime_->isAvailable())
        spdlog::info("QPythonRuntime: Python 运行环境就绪，活动会话已注入 precess.current");
    else
        spdlog::error("QPythonRuntime: {}", runtime_->lastError());
    emit availableChanged();
}

void QPythonRuntime::ensureInitialized()
{
    if (!runtime_->isAvailable() && runtime_->lastError().empty())
        initialize();
}

QVariantMap QPythonRuntime::execute(const QString& source)
{
    QVariantMap result;
    result["ok"] = false;
    result["incomplete"] = false;
    result["output"] = QString();
    result["error"] = QString();
    Q_ASSERT(QThread::currentThread() == thread()); // Python ≡ GUI 线程

    ensureInitialized();
    const python::Runtime::ExecutionResult executed = runtime_->execute(source.toStdString());
    result["ok"] = executed.ok;
    result["incomplete"] = executed.incomplete;
    result["output"] = QString::fromStdString(executed.output);
    result["error"] = QString::fromStdString(executed.error);
    return result;
}

QString QPythonRuntime::version() const
{
    return QString::fromStdString(runtime_->version());
}
