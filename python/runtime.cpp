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

namespace py = pybind11;

namespace python {

struct Runtime::State {
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
    if (!state_->initialized)
        return;
    // 先丢弃 Python 侧活会话引用再终结解释器。GIL 自初始化起归初始化线程
    // 所有：acquire 守卫在同线程为无操作配对，块结束时 GIL 仍被持有，可直接
    // Finalize；终结之后不再触碰任何 Python API
    {
        py::gil_scoped_acquire gil;
        state_->precess_module.attr("current") = py::none();
        state_->precess_module = py::object();
    }
    Py_FinalizeEx();
}

bool Runtime::isAvailable() const
{
    return state_->initialized;
}

const std::string& Runtime::lastError() const
{
    return state_->last_error;
}

void Runtime::initialize()
{
    if (state_->initialized || !state_->last_error.empty())
        return;

    // 1) 解释器初始化：python_home 非空时经 PyConfig 把标准库根固定为该目录。
    //    嵌入场景主程序不是 python.exe，不显式给 home 时标准库定位依赖环境
    //    变量等启发式，跨部署形态不可靠
    PyConfig config;
    PyConfig_InitPythonConfig(&config);
    if (!state_->config.python_home.empty())
        PyConfig_SetString(&config, &config.home, state_->config.python_home.wstring().c_str());
    if (!state_->config.program_name.empty())
        PyConfig_SetBytesString(&config, &config.program_name, state_->config.program_name.c_str());
    const PyStatus status = Py_InitializeFromConfig(&config);
    PyConfig_Clear(&config);
    if (PyStatus_Exception(status)) {
        state_->last_error = std::string("Python 解释器初始化失败：")
            + (status.err_msg ? status.err_msg : "未知错误");
        return;
    }

    // 2) sys.path 按优先级前置模块目录后导入 precess，并注入活会话（引用
    //    策略，无所有权）。初始化后 GIL 归初始化线程，此后同线程的 gil 守卫
    //    均为无操作配对
    py::gil_scoped_acquire gil;
    try {
        py::module_ sys = py::module_::import("sys");
        // 逆序前置，使 module_dirs 首个候选位于 sys.path 最前
        for (auto it = state_->config.module_dirs.rbegin(); it != state_->config.module_dirs.rend(); ++it)
            sys.attr("path").attr("insert")(0, it->u8string());
        state_->precess_module = py::module_::import("precess");
        state_->precess_module.attr("current")
            = py::cast(state_->session, py::return_value_policy::reference);
        state_->initialized = true;
    } catch (const py::error_already_set& e) {
        state_->last_error = std::string("precess 模块导入失败：")
            + e.what()
            + "\n（请确认构建已生成 precess 扩展模块，且位于 module_dirs 候选目录之一）";
        state_->precess_module = py::object();
        return;
    }

    // 3) 控制台辅助注入（失败仅记日志，不影响运行时可用性）：控制台输入行是
    //    纯 Python、无界面级命令，help/clear/exit 以 Python 函数覆盖 site 默认——
    //    pydoc 的交互模式依赖 stdin，嵌入环境没有可交互 stdin（help() 会以
    //    "lost sys.stdin" 崩出），裸 help 的 "Type help()..." 提示语也会误导用户；
    //    clear() 经输出换页符 \f、由界面识别清屏
    try {
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
}

void Runtime::ensureInitialized()
{
    if (!state_->initialized && state_->last_error.empty())
        initialize();
}

Runtime::ExecutionResult Runtime::execute(const std::string& source)
{
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
