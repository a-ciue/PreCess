#ifndef PYTHON_RUNTIME_H
#define PYTHON_RUNTIME_H

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace session {
class Session;
}

namespace python {

/**
 * @brief 内嵌 Python 运行时宿主：解释器生命周期、precess 导入与控制台执行
 *
 * 无 Qt，供 GUI（app/model 的 QPythonRuntime）与无头宿主（CLI）共用。
 * 线程约定：同一实例的所有方法须在同一线程调用（GUI 宿主即 GUI 线程），
 * 渲染/工作线程不得触碰。
 *
 * 生命周期约定：经引用策略把 session 注入 precess.current（Python 侧不持有
 * 所有权），session 必须比本对象活得久；析构先丢弃引用再终结解释器。
 *
 * 初始化为幂等懒式：config.python_home 非空时经 PyConfig 把标准库根固定到
 * 该目录（嵌入场景主程序不是 python.exe，默认定位不可靠），向 sys.path 前置
 * config.module_dirs 候选后导入 precess。失败后记录原因且不再重试。
 */
class Runtime {
public:
    struct Config {
        std::string program_name { "PreCess" };         //> 解释器程序名标识
        std::filesystem::path python_home;              //> 标准库根（留空交由解释器默认定位）
        std::vector<std::filesystem::path> module_dirs; //> precess 扩展模块目录候选（按优先级，依次前置 sys.path）
    };

    struct ExecutionResult {
        bool ok { false };         //> 是否已执行且成功
        bool incomplete { false }; //> 源码是否未完（等待续行，未执行）
        std::string output;        //> stdout/stderr 捕获（UTF-8）
        std::string error;         //> 异常描述（traceback 全文，UTF-8）
    };

    /**
     * @brief 构造宿主，仅记录配置与会话，不启动解释器（懒初始化）
     * @param session 活会话，注入 precess.current，须比本对象活得久
     * @param config 解释器配置
     */
    Runtime(session::Session* session, Config config);
    /**
     * @brief 析构：丢弃 Python 侧活会话引用并终结解释器
     */
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    /**
     * @brief 解释器是否就绪（初始化成功且 precess 模块可用）
     */
    bool isAvailable() const;
    /**
     * @brief 最近一次初始化失败的描述（UTF-8），空串表示无失败记录
     */
    const std::string& lastError() const;

    /**
     * @brief 初始化解释器并导入 precess 模块（幂等；失败后不重试）
     */
    void initialize();
    /**
     * @brief 执行一段控制台源码（与交互式解释器同判定规则，未初始化时先懒初始化）
     * @param source 待执行源码（UTF-8），可为多行未完块（未完时只做编译判定、不执行）
     * @return ok 是否已执行且成功；incomplete 源码是否未完（等待续行）；
     *         output stdout/stderr 捕获；error 异常描述（traceback 全文）
     */
    ExecutionResult execute(const std::string& source);
    /**
     * @brief 解释器版本串（sys.version，UTF-8），不可用时返回空串
     */
    std::string version() const;

private:
    void ensureInitialized(); //> execute 的懒初始化入口，仅未初始化且无失败记录时才真正初始化

    struct State; //> PIMPL：隔离 pybind11/CPython 头文件
    std::unique_ptr<State> state_; //> 解释器与模块句柄
};

}

#endif
