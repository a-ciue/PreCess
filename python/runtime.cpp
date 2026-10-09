/**
 * @file runtime.cpp
 * @brief python::Runtime 实现：pybind11 嵌入式解释器宿主（无 Qt）
 *
 * 本编译单元不含 Qt 头文件，无 slots/signals/emit 宏冲突问题；上层宿主
 * （如 QPythonRuntime）经 PIMPL 头文件隔离，同样不受影响。线程与生命周期
 * 约定见 python/runtime.h 类注释。
 */
#include "python/runtime.h"

#include <pybind11/embed.h>

#include "Session.h"

#include <spdlog/spdlog.h>

#include <utility>
#include <thread>
#include <stdexcept>

namespace py = pybind11;

namespace python {

#ifdef PRECESS_RUNTIME_FAILURE_TEST
// 仅故障测试目标提供定义；生产库没有此符号、状态或公开 API。
void runtimeInitializationCheckpoint(const char* stage);
#endif

struct Runtime::State {
    const std::thread::id owner_thread = std::this_thread::get_id();
    bool interpreter_owned = false;
    bool initialization_attempted = false;
    std::unique_ptr<py::gil_scoped_release> idle_gil;
    void checkThread() const
    {
        if (std::this_thread::get_id() != owner_thread)
            throw std::logic_error("Python Runtime must be used on its owning thread");
    }
    session::Session* session = nullptr; //> 活会话（不持有所有权，宿主保证生命周期）
    Config config;                       //> 解释器配置（标准库根与模块目录候选）
    bool initialized = false;            //> 解释器就绪且 precess 已导入
    bool executing = false;              //> 重入守卫：Python 执行期间拒绝再次进入
    std::string last_error;              //> 初始化失败描述；非空时不再重试
    py::object precess_module;           //> 已导入的 precess 绑定模块（current 挂活会话）
};

Runtime::Runtime(session::Session* session, Config config)
    : state_(std::make_unique<State>())
{
    state_->session = session;
    state_->config = std::move(config);
}

Runtime::~Runtime()
{
    if (!state_->interpreter_owned)
        return;
    // 析构不能跨线程恢复状态；违反宿主契约时拒绝继续操作解释器。
    if (std::this_thread::get_id() != state_->owner_thread || state_->executing)
        std::terminate();
    state_->idle_gil.reset(); // 恢复初始化线程状态；此后保持 GIL 至终结。
    {
        if (state_->precess_module) {
            try {
                state_->precess_module.attr("current") = py::none();
            } catch (const py::error_already_set& e) {
                spdlog::error("Python Runtime shutdown: {}", e.what());
            }
        }
        state_->precess_module = py::object();
    }
    py::finalize_interpreter();
}

bool Runtime::isAvailable() const
{
    state_->checkThread();
    return state_->initialized;
}

const std::string& Runtime::lastError() const
{
    state_->checkThread();
    return state_->last_error;
}

void Runtime::initialize()
{
    state_->checkThread();
    if (state_->initialization_attempted)
        return;
    state_->initialization_attempted = true;

    // 守卫在所有 Python 局部对象和 catch 异常对象之外：异常报告自身抛异常
    // 时也会回滚。恢复 GIL -> 清空引用 -> 终结，绝不让 release 跨越终结。
    struct InitializationRollback {
        State& state;
        ~InitializationRollback()
        {
            if (!state.initialized && state.interpreter_owned) {
                state.idle_gil.reset();
                state.precess_module = py::object();
                py::finalize_interpreter();
                state.interpreter_owned = false;
            }
        }
    } rollback { *state_ };

    // 1) 解释器初始化：python_home 非空时经 PyConfig 把标准库根固定为该目录。
    //    嵌入场景主程序不是 python.exe，不显式给 home 时标准库定位依赖环境
    //    变量等启发式，跨部署形态不可靠
    PyConfig config;
    PyConfig_InitPythonConfig(&config);
    struct ConfigCleanup {
        PyConfig& config;
        bool delegated = false;
        ~ConfigCleanup() { if (!delegated) PyConfig_Clear(&config); }
    } config_cleanup { config };
    try {
    auto check_config = [&](PyStatus status) {
        if (!PyStatus_Exception(status))
            return true;
        state_->last_error = status.err_msg ? status.err_msg : "Python config failed";
        return false;
    };
    if (!state_->config.python_home.empty()
        && !check_config(PyConfig_SetString(&config, &config.home, state_->config.python_home.wstring().c_str())))
        return;
    if (!state_->config.program_name.empty()
        && !check_config(PyConfig_SetBytesString(&config, &config.program_name, state_->config.program_name.c_str())))
        return;
    if (Py_IsInitialized()) {
        state_->last_error = "Python interpreter already exists; Runtime requires exclusive ownership";
        return;
    }
        // 此重载负责清理 PyConfig；禁止自动改变模块搜索路径。
        config_cleanup.delegated = true;
        py::initialize_interpreter(&config, 0, nullptr, false);
        state_->interpreter_owned = true;
#ifdef PRECESS_RUNTIME_FAILURE_TEST
        runtimeInitializationCheckpoint("import");
#endif

    // 2) sys.path 按优先级前置模块目录后导入 precess，并注入活会话（引用
    //    策略，无所有权）。初始化期间持 GIL，全部 Python 临时对象清理后再释放。
    {
        py::module_ sys = py::module_::import("sys");
        // 逆序前置，使 module_dirs 首个候选位于 sys.path 最前
        for (auto it = state_->config.module_dirs.rbegin(); it != state_->config.module_dirs.rend(); ++it)
            sys.attr("path").attr("insert")(0, it->u8string());
        state_->precess_module = py::module_::import("precess");
        state_->precess_module.attr("current")
            = py::cast(state_->session, py::return_value_policy::reference);
    }

    // 3) 控制台辅助注入（失败仅记日志，不影响运行时可用性）：控制台输入行是
    //    纯 Python、无界面级命令，help/clear/exit 以 Python 函数覆盖 site 默认——
    //    pydoc 的交互模式依赖 stdin，嵌入环境没有可交互 stdin（help() 会以
    //    "lost sys.stdin" 崩出），裸 help 的 "Type help()..." 提示语也会误导用户；
    //    clear() 经输出换页符 \f、由界面识别清屏
    try {
#ifdef PRECESS_RUNTIME_FAILURE_TEST
        runtimeInitializationCheckpoint("console");
#endif
        py::exec(R"PY(
def _console_guide():
    return (
        "PreCess Python 控制台:\n"
        "- 输入即 Python，回车执行；def/for/if 未完时提示 ... 续行，Esc 放弃\n"
        "- import precess 后 precess.current 即当前 GUI 会话\n"
        "- 查询: precess.current.query.list_models()\n"
        "- 功能: precess.current.call('CreateBox', 0, 0, 0, 5, 5, 5, 2)\n"
        "- 自省: precess.current.feature_params('CreateBox')\n"
        "- 撤销/重做: precess.current.undo_stack.undo() / redo()\n"
        "- clear() 清空窗口；help(对象) 查看文档（如 help(str)）\n"
        "- exit()/quit() 仅作提示，GUI 程序请直接关闭窗口退出")

class _ConsoleHelp:
    def __call__(self, *args):
        import pydoc
        if args:
            pydoc.doc(*args)
        else:
            print(_console_guide())

    def __repr__(self):
        return _console_guide()

class _ConsoleExit:
    def __call__(self):
        print("PreCess 是 GUI 程序，请直接关闭窗口退出")

    def __repr__(self):
        return "PreCess 是 GUI 程序，请直接关闭窗口退出"

def _console_clear():
    import sys
    sys.stdout.write('\f')

help = _ConsoleHelp()
exit = quit = _ConsoleExit()
clear = _console_clear
del _ConsoleHelp, _ConsoleExit, _console_clear
)PY",
            py::module_::import("__main__").attr("__dict__"));
    } catch (const py::error_already_set& e) {
        spdlog::error("python::Runtime: 控制台辅助注入失败: {}", e.what());
    }
    // 所有局部 Python 对象已析构。守卫跨越 GUI 空闲期，不能是局部变量。
#ifdef PRECESS_RUNTIME_FAILURE_TEST
    runtimeInitializationCheckpoint("before_release");
#endif
    state_->idle_gil = std::make_unique<py::gil_scoped_release>();
#ifdef PRECESS_RUNTIME_FAILURE_TEST
    runtimeInitializationCheckpoint("after_release");
#endif
    state_->initialized = true; // 最后发布可用状态；之后只析构原生配置守卫。
    } catch (const py::error_already_set& e) {
        state_->last_error = std::string("Python 初始化失败：") + e.what();
    } catch (const std::exception& e) {
        state_->last_error = std::string("Python 初始化失败：") + e.what();
    } catch (...) {
        state_->last_error = "Python 初始化失败：未知 C++ 异常";
    }
}

void Runtime::ensureInitialized()
{
    if (!state_->initialized && state_->last_error.empty())
        initialize();
}

Runtime::ExecutionResult Runtime::execute(const std::string& source)
{
    state_->checkThread();
    ExecutionResult result;
    ensureInitialized();
    if (!state_->initialized) {
        result.error = state_->last_error;
        return result;
    }
    if (state_->executing) {
        result.error = "Python 正在执行中，拒绝重入调用";
        return result;
    }

    py::gil_scoped_acquire gil;
    state_->executing = true;
    struct ExecutionGuard {
        bool& flag;
        ~ExecutionGuard() { flag = false; }
    } execution_guard { state_->executing };
    std::string captured;
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
                source, "<console>", "single");
            if (code.is_none()) {
                result.incomplete = true;
            } else {
                // single 模式下表达式结果经 sys.displayhook 打印，随输出一起捕获；
                // 控制台共享 __main__ 命名空间，变量跨多次执行存活
                py::module_::import("builtins").attr("exec")(
                    code, py::module_::import("__main__").attr("__dict__"));
                result.ok = true;
            }
        } catch (const py::error_already_set& e) {
            // traceback 全量格式化（含语法错误定位），观感与交互式解释器一致
            try {
                py::object formatted = py::module_::import("traceback").attr("format_exception")(
                    e.type(), e.value(), e.trace());
                result.error = static_cast<std::string>(py::str(py::str("\n").attr("join")(formatted)));
            } catch (const py::error_already_set&) {
                result.error = e.what();
            }
        }
        // 无论成败都还原标准流并取回捕获内容
        sys.attr("stdout") = old_stdout;
        sys.attr("stderr") = old_stderr;
        captured = static_cast<std::string>(py::str(buffer.attr("getvalue")()));
    } catch (const py::error_already_set& e) {
        // 兜底：流管理自身出错时保证错误可见
        result.error = e.what();
    }
    state_->executing = false;
    result.output = std::move(captured);
    return result;
}

std::string Runtime::version() const
{
    state_->checkThread();
    if (!state_->initialized)
        return std::string();
    py::gil_scoped_acquire gil;
    try {
        return static_cast<std::string>(py::str(py::module_::import("sys").attr("version")));
    } catch (const py::error_already_set&) {
        return std::string();
    }
}

}
