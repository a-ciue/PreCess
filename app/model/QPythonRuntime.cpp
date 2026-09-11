/**
 * @file QPythonRuntime.cpp
 * @brief QPythonRuntime 实现：pybind11 嵌入式解释器宿主
 *
 * 仅在 PRECESS_EMBED_PYTHON（app/model/CMakeLists 检测到 Python3 + pybind11 且
 * 非 wasm 构建）下编译真实实现，否则降级为恒不可用。线程与生命周期约定见
 * QPythonRuntime.h 类注释。
 */
#include "QPythonRuntime.h"

#ifdef PRECESS_EMBED_PYTHON

#ifndef NOMINMAX
#define NOMINMAX
#endif
// Qt 的 qobjectdefs.h 把 slots/signals/emit 定义为宏，会破坏 CPython 头文件中
// 的 PyType_Spec::slots 成员与 pybind11 的 PyType_Spec 初始化——引入 Python 期间
// 取消，处理完 Python/pybind11 头后原样还原（本编译单元其后仍用 Qt 关键字宏）
#pragma push_macro("slots")
#pragma push_macro("signals")
#pragma push_macro("emit")
#undef slots
#undef signals
#undef emit
#include <pybind11/embed.h>
#pragma pop_macro("emit")
#pragma pop_macro("signals")
#pragma pop_macro("slots")

#include "Session.h"

#include <QCoreApplication>
#include <QThread>
#include <spdlog/spdlog.h>

#include <vector>

namespace py = pybind11;

namespace {

//> precess 扩展模块的 sys.path 注入候选：优先部署形态 <exe_dir>/python，
//> 其次构建树 precess 目标输出目录（CMake 期给定）
std::vector<std::string> moduleDirCandidates()
{
    return {
        (QCoreApplication::applicationDirPath() + QStringLiteral("/python")).toStdString(),
        std::string(PRECESS_PYTHON_MODULE_DIR),
    };
}

} // namespace

struct QPythonRuntime::State {
    session::Session* session = nullptr; //> 活会话（不持有所有权，QModelManager 保证生命周期）
    bool initialized = false; //> 解释器就绪且 precess 已导入
    bool executing = false; //> 重入守卫：Python 执行期间拒绝再次进入
    QString last_error; //> 初始化失败描述；非空时不再重试
    py::object precess_module; //> 已导入的 precess 绑定模块（current 挂活会话）
};

QPythonRuntime::QPythonRuntime(session::Session* session, QObject* parent)
    : QObject(parent)
    , state_(std::make_unique<State>())
{
    state_->session = session;
}

QPythonRuntime::~QPythonRuntime()
{
    if (!state_->initialized)
        return;
    // 先丢弃 Python 侧活会话引用再终结解释器。GIL 自初始化起归本线程（GUI 线程）
    // 所有：acquire 守卫在同线程为无操作配对，块结束时 GIL 仍被持有，可直接
    // Finalize；终结之后不再触碰任何 Python API
    {
        py::gil_scoped_acquire gil;
        state_->precess_module.attr("current") = py::none();
        state_->precess_module = py::object();
    }
    Py_FinalizeEx();
}

bool QPythonRuntime::isAvailable() const
{
    return state_->initialized;
}

QString QPythonRuntime::lastError() const
{
    return state_->last_error;
}

void QPythonRuntime::initialize()
{
    Q_ASSERT(QThread::currentThread() == thread()); // Python ≡ GUI 线程（类注释）
    ensureInitialized();
}

void QPythonRuntime::ensureInitialized()
{
    if (state_->initialized || !state_->last_error.isEmpty())
        return;

    // 1) 解释器初始化：标准库根经 PyConfig 固定为 CMake 期绑定的解释器目录。
    //    嵌入场景主程序不是 python.exe，不显式给 home 时标准库定位依赖环境变量
    //    等启发式，跨部署形态不可靠；路径以正斜杠给出，可安全嵌入 C 字符串
    PyConfig config;
    PyConfig_InitPythonConfig(&config);
    const std::wstring home = QString::fromUtf8(PRECESS_PYTHON_HOME).toStdWString();
    PyConfig_SetString(&config, &config.home, home.c_str());
    const std::wstring program_name = QStringLiteral("PreCess").toStdWString();
    PyConfig_SetString(&config, &config.program_name, program_name.c_str());
    const PyStatus status = Py_InitializeFromConfig(&config);
    PyConfig_Clear(&config);
    if (PyStatus_Exception(status)) {
        state_->last_error = QStringLiteral("Python 解释器初始化失败：")
            + QString::fromUtf8(status.err_msg ? status.err_msg : "未知错误");
        spdlog::error("QPythonRuntime: {}", state_->last_error.toStdString());
        emit availableChanged();
        return;
    }

    // 2) sys.path 注入模块目录后导入 precess，并注入活会话（引用策略，无所有权）。
    //    初始化后 GIL 归本线程（GUI 线程），此后同线程的 gil 守卫均为无操作配对
    py::gil_scoped_acquire gil;
    try {
        py::module_ sys = py::module_::import("sys");
        for (const std::string& dir : moduleDirCandidates())
            sys.attr("path").attr("insert")(0, dir);
        state_->precess_module = py::module_::import("precess");
        state_->precess_module.attr("current")
            = py::cast(state_->session, py::return_value_policy::reference);
        state_->initialized = true;
        spdlog::info("QPythonRuntime: Python 运行环境就绪，活动会话已注入 precess.current");
    } catch (const py::error_already_set& e) {
        state_->last_error = QStringLiteral("precess 模块导入失败：")
            + QString::fromUtf8(e.what())
            + QStringLiteral("\n（请确认构建目录 python/ 下已生成 precess 扩展模块，"
                             "或经 PreCess-deps.py 安装 Python/pybind11 依赖）");
        state_->precess_module = py::object();
        spdlog::error("QPythonRuntime: {}", state_->last_error.toStdString());
    }
    emit availableChanged();
}

QVariantMap QPythonRuntime::execute(const QString& source)
{
    QVariantMap result;
    result["ok"] = false;
    result["incomplete"] = false;
    result["output"] = QString();
    result["error"] = QString();
    Q_ASSERT(QThread::currentThread() == thread()); // Python ≡ GUI 线程（类注释）

    ensureInitialized();
    if (!state_->initialized) {
        result["error"] = state_->last_error;
        return result;
    }
    if (state_->executing) {
        result["error"] = QStringLiteral("Python 正在执行中，拒绝重入调用");
        return result;
    }

    py::gil_scoped_acquire gil;
    state_->executing = true;
    QString captured;
    try {
        // Python 侧 sys.stdout/sys.stderr 临时换成 StringIO 捕获输出（含表达式
        // 结果的 displayhook 打印），还原后经 getvalue() 取回内容
        py::module_ sys = py::module_::import("sys");
        py::object buffer = py::module_::import("io").attr("StringIO")();
        py::object old_stdout = sys.attr("stdout");
        py::object old_stderr = sys.attr("stderr");
        sys.attr("stdout") = buffer;
        sys.attr("stderr") = buffer;
        try {
            // 与交互式解释器同判定：codeop.compile_command 未完返回 None，
            // 语法错误抛 SyntaxError/ValueError/OverflowError
            py::object code = py::module_::import("codeop").attr("compile_command")(
                source.toStdString(), "<console>", "single");
            if (code.is_none()) {
                result["incomplete"] = true;
            } else {
                // 控制台共享 __main__ 命名空间，变量跨多次执行存活
                py::module_::import("builtins").attr("exec")(code, py::module_::import("__main__").attr("__dict__"));
                result["ok"] = true;
            }
        } catch (const py::error_already_set& e) {
            // traceback 全量格式化（含语法错误定位），观感与交互式解释器一致
            try {
                py::object formatted = py::module_::import("traceback").attr("format_exception")(e.type(), e.value(), e.trace());
                result["error"] = QString::fromStdString(
                    static_cast<std::string>(py::str(py::str("\n").attr("join")(formatted))));
            } catch (const py::error_already_set&) {
                result["error"] = QString::fromUtf8(e.what());
            }
        }
        // 无论成败都还原标准流并取回捕获内容
        sys.attr("stdout") = old_stdout;
        sys.attr("stderr") = old_stderr;
        captured = QString::fromStdString(
            static_cast<std::string>(py::str(buffer.attr("getvalue")())));
    } catch (const py::error_already_set& e) {
        // 兜底：流管理自身出错时保证错误可见
        result["error"] = QString::fromUtf8(e.what());
    }
    state_->executing = false;
    result["output"] = captured;
    return result;
}

QString QPythonRuntime::version() const
{
    if (!state_->initialized)
        return QString();
    py::gil_scoped_acquire gil;
    try {
        return QString::fromStdString(
            static_cast<std::string>(py::str(py::module_::import("sys").attr("version"))));
    } catch (const py::error_already_set&) {
        return QString();
    }
}

#else // 非 PRECESS_EMBED_PYTHON：无 Python 环境的降级实现（如 wasm 构建）

struct QPythonRuntime::State { };

QPythonRuntime::QPythonRuntime(session::Session* session, QObject* parent)
    : QObject(parent)
    , state_(std::make_unique<State>())
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
