/**
 * @file QPythonAppModule.cpp
 * @brief app 侧 Python 模块注册（precess_app）：pybind11 任意签名 def + QTimer 定时器表
 *
 * 注册逻辑全部在 app（真实现编入本文件，桩模式编入 QPythonAppModule_stub.cpp，
 * 由 CMake 按 precess_bindings 目标存在性二选一）：经 PyImport_AddModule 创建
 * app 专属可 import 模块 precess_app，本机函数以任意 C++ 签名经 pybind11 类型
 * 转换器暴露，脚本回调以 py::function 登记入定时器表。定时策略（QTimer 单发
 * 定时器表）、回调唤起与生命周期全由本文件管理：回调约定在 GUI 主线程执行
 * （解释器 GIL 归初始化线程，acquire 为无操作配对），异常全量格式化记日志
 * 消化；关停须先于解释器终结（py::object 先释放）。
 *
 * Qt 与 pybind11 头同编译单元：Qt 的 slots/signals/emit 关键字宏会破坏 CPython
 * 的 PyType_Spec::slots 等声明，include 前取消、后还原（AGENTS.md §10）。
 */
#include "QPythonAppModule.h"

#include <QObject>
#include <QThread>
#include <QTimer>

#include <spdlog/spdlog.h>

#pragma push_macro("slots")
#pragma push_macro("signals")
#pragma push_macro("emit")
#undef slots
#undef signals
#undef emit
#include <pybind11/gil.h>
#include <pybind11/pybind11.h>
#pragma pop_macro("emit")
#pragma pop_macro("signals")
#pragma pop_macro("slots")

#include <climits>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace py = pybind11;

namespace python_app {

namespace {

    // traceback 全量格式化（含语法错误定位），延时回调异常报告用
    std::string formatException(const py::error_already_set& e)
    {
        try {
            py::object formatted = py::module_::import("traceback").attr("format_exception")(e.type(), e.value(), e.trace());
            return static_cast<std::string>(py::str(py::str("\n").attr("join")(formatted)));
        } catch (const py::error_already_set&) {
            return e.what();
        }
    }

    //! @brief 定时器条目：单发 QTimer 与脚本回调（py::object 须在解释器终结前释放）
    struct TimeoutEntry {
        QTimer* timer; //> 单发定时器（parent 挂宿主 QObject）
        py::object callback; //> 脚本回调
    };

    //! @brief QTimer 定时器表：延时策略的 app 侧全权管理者——登记建单发定时器，
    //! 触发或取消即终结；定时器 parent 挂宿主对象，随宿主析构
    struct TimeoutTable {
        explicit TimeoutTable(QObject* owner)
            : owner(owner)
        {
        }

        //! @brief 析构：停掉并销毁全部未触发定时器（py::object 析构须持 GIL，见 shutdownAppModule）
        ~TimeoutTable()
        {
            for (auto& [id, entry] : entries) {
                entry.timer->stop();
                delete entry.timer;
            }
        }

        /**
         * @brief 登记脚本回调并安排单发定时器（precess_app.call_later 实现），返回调用 id
         *
         * 仅解释器线程（GUI 主线程）调用：脚本经 pybind 调入时 GIL 已被本线程持有
         */
        long long schedule(long long delay_ms, py::function callback)
        {
            Q_ASSERT(QThread::currentThread() == owner->thread()); // 定时器 ≡ GUI 线程
            if (delay_ms < 0 || delay_ms > INT_MAX)
                throw std::invalid_argument("delay_ms 须在 0 ~ 2147483647 毫秒之间");
            const uint64_t id = next_id++;
            auto* timer = new QTimer(owner);
            timer->setSingleShot(true);
            QObject::connect(timer, &QTimer::timeout, owner, [this, id]() { fire(id); });
            timer->start(static_cast<int>(delay_ms));
            entries.emplace(id, TimeoutEntry { timer, std::move(callback) });
            return static_cast<long long>(id);
        }

        /**
         * @brief 取消未触发回调（precess_app.cancel_call 实现），返回是否成功移除
         */
        bool cancel(long long id)
        {
            const auto it = entries.find(static_cast<uint64_t>(id));
            if (it == entries.end())
                return false;
            it->second.timer->stop();
            it->second.timer->deleteLater();
            entries.erase(it);
            return true;
        }

        /**
         * @brief 定时器触发：单发即终结，随后唤起脚本回调（异常记日志消化）
         */
        void fire(uint64_t id)
        {
            const auto it = entries.find(id);
            if (it == entries.end())
                return;
            // GIL 归初始化线程（GUI 主线程）所有，此处 acquire 为无操作配对；
            // py::object 的搬运与析构均须持 GIL，故守卫先于回调取出
            py::gil_scoped_acquire gil;
            py::object callback = std::move(it->second.callback);
            it->second.timer->deleteLater();
            entries.erase(it);
            try {
                callback();
            } catch (const py::error_already_set& e) {
                spdlog::error("precess_app 延时回调执行失败:\n{}", formatException(e));
            }
        }

        QObject* owner; //> 定时器 parent 与线程归属（不持有所有权）
        uint64_t next_id = 1; //> 调用 id 发号器
        std::map<uint64_t, TimeoutEntry> entries; //> 调用 id → 未触发条目
    };

    std::unique_ptr<TimeoutTable> timeout_table; //> 定时器表（register/shutdown 成对创建销毁）

} // namespace

void registerAppModule(QObject* owner)
{
    Q_ASSERT(QThread::currentThread() == owner->thread()); // Python ≡ GUI 线程
    timeout_table = std::make_unique<TimeoutTable>(owner);
    try {
        py::gil_scoped_acquire gil;
        // PyImport_AddModule 创建模块并登记 sys.modules（import 经 sys.modules
        // 命中）；返回借用引用必须 borrow。勿用 create_extension_module——那是
        // 静态编译期扩展模块专用（走扩展缓存，运行期创建会在 import 时段错误）。
        // 同进程重建运行时时会取回同名旧模块并覆盖注册
        py::module_ module = py::reinterpret_borrow<py::module_>(PyImport_AddModule("precess_app"));
        module.attr("__doc__")
            = "PreCess app 侧功能模块（GUI 宿主经 pybind11 注册，app 侧 Python 函数"
              "统一收在本模块，后续新增函数对模块句柄继续 def 即可）";
        // —— 延时回调：定时策略在 app，回调以 py::function 登记入定时器表 ——
        module.def("call_later", [](long long delay_ms, py::function callback) { return timeout_table->schedule(delay_ms, std::move(callback)); }, py::arg("delay_ms"), py::arg("callback"), "延时注册：登记 callback（无参回调），由 GUI 主线程延迟 delay_ms 毫秒后调用，"
                                                                                                                                                                                             "返回调用 id；回调里的异常经日志报告，不会中断宿主");
        module.def("cancel_call", [](long long id) { return timeout_table->cancel(id); }, py::arg("id"), "取消未触发的延时调用，返回是否成功移除");
        // —— 任意签名示例：pybind11 全套类型转换器可用（多参、默认值、容器、
        //    重载等），后续 app 侧函数照此形状继续 def ——
        module.def("echo", [](const std::string& text, int times) {
                std::string result;
                result.reserve(text.size() * static_cast<std::size_t>(times));
                for (int i = 0; i < times; ++i)
                    result += text;
                return result; }, py::arg("text"), py::arg("times") = 1, "任意签名示例：把 text 重复 times 次返回");
    } catch (const py::error_already_set& e) {
        spdlog::error("python_app: 注册 precess_app 模块失败: {}", e.what());
    }
}

void shutdownAppModule()
{
    if (!timeout_table)
        return;
    // py::object（脚本回调）析构 decref 须持 GIL，故在守卫内置空
    py::gil_scoped_acquire gil;
    timeout_table.reset();
}

}
