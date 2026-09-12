/**
 * @file QPythonRuntime.cpp
 * @brief QPythonRuntime 实现：python::Runtime 之上的 QObject/QML 薄壳
 *
 * 解释器宿主逻辑在 python/src/runtime.cpp（无 Qt）；本文件只做线程断言、
 * 字符串编解码、日志与信号桥接。仅在 PRECESS_EMBED_PYTHON（app/model/
 * CMakeLists 检测到 precess_runtime 目标且非 wasm 构建）下持有宿主实例，
 * 否则降级为恒不可用。
 */
#include "QPythonRuntime.h"

#ifdef PRECESS_EMBED_PYTHON

#include "python/runtime.h"

#include <QCoreApplication>
#include <QThread>
#include <spdlog/spdlog.h>

#include <filesystem>

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

#else // 非 PRECESS_EMBED_PYTHON：无 Python 环境的降级实现（如 wasm 构建）

QPythonRuntime::QPythonRuntime(session::Session* session, QObject* parent)
    : QObject(parent)
{
    Q_UNUSED(session)
}

QPythonRuntime::~QPythonRuntime() = default;

bool QPythonRuntime::isAvailable() const
{
    return false;
}

QString QPythonRuntime::lastError() const
{
    return QStringLiteral(
        "编译时未启用内嵌 Python 运行环境（需要 Python3 与 pybind11；wasm 构建不支持）");
}

void QPythonRuntime::initialize() { }

QVariantMap QPythonRuntime::execute(const QString& source)
{
    Q_UNUSED(source)
    QVariantMap result;
    result["ok"] = false;
    result["incomplete"] = false;
    result["output"] = QString();
    result["error"] = lastError();
    return result;
}

QString QPythonRuntime::version() const
{
    return QString();
}

#endif
