/**
 * @file CgalExceptionGuard.h
 * @brief CGAL 断言失败行为 RAII 守卫：作用域内将 abort 切换为抛 C++ 异常
 *
 * 本库依赖 CGAL（GPLv3）。按项目许可证分界，仅允许 plugins/（AGPLv3）内使用，
 * 禁止 core/、model/、cmake/ 目录依赖本库或 CGAL 头文件。
 *
 * 用法：在调 PMP/CGAL 算法的代码块顶部声明 `CgalExceptionGuard guard;`，
 *       作用域内所有 CGAL_assertion / CGAL_error 触发时抛
 *       CGAL::Failure_exception 链（Assertion_exception / Precondition_exception /
 *       Error_exception），被外层 try/catch 统一转成温和文案。
 *
 * 退出作用域自动恢复 CGAL 默认的 ABORT 行为，不影响其他使用 CGAL 的代码。
 */
#ifndef CGAL_EXCEPTION_GUARD_H
#define CGAL_EXCEPTION_GUARD_H

#include <CGAL/assertions_behaviour.h>
#include <CGAL/exceptions.h>

namespace cgalsupport {

/**
 * @brief RAII 守卫：构造时将 CGAL 错误行为切到 THROW_EXCEPTION，析构时还原
 *
 * CGAL 默认失败行为是 ABORT（直接 std::abort + stderr 打印内部表达式如
 * `Q.empty() || (Q.front() == Q.back())`），对 UI 用户不友好且会直接终止程序。
 * 本类在作用域内临时切到 THROW_EXCEPTION，使所有 CGAL_assertion / CGAL_error
 * 触发时抛 CGAL::Failure_exception 链，可被外层 try/catch 捕获并转成业务文案。
 *
 * @warning CGAL 的 Failure_behaviour 是进程级全局状态，本守卫切换全局并在
 *          析构时还原。**当前假设使用方在单线程同步路径下执行**（如 GUI 线程
 *          同步调用）；若将来出现并发 CGAL 入口（多线程 / 多插件并行调用 PMP），
 *          必须在调用方用互斥锁或线程局部存储隔离，否则互相覆盖 error_behaviour。
 *
 * @code
 *   try {
 *       CgalExceptionGuard guard;
 *       PMP::triangulate_hole(sm, ...);  // 失败 → 抛 Failure_exception
 *   } catch (const CGAL::Failure_exception& e) {
 *       spdlog::error("CGAL 拓扑违反: {}", e.expression());
 *       return std::string("修复失败：网格拓扑不符合 PMP 前提");
 *   }
 * @endcode
 */
class CgalExceptionGuard {
public:
    /**
     * @brief 构造：保存当前 CGAL 错误行为并切到 THROW_EXCEPTION
     */
    CgalExceptionGuard()
        : prev_behaviour_(CGAL::set_error_behaviour(CGAL::THROW_EXCEPTION))
    {
    }

    /**
     * @brief 析构：恢复构造前保存的 CGAL 错误行为（通常为 ABORT）
     */
    ~CgalExceptionGuard()
    {
        CGAL::set_error_behaviour(prev_behaviour_);
    }

    CgalExceptionGuard(const CgalExceptionGuard&) = delete;
    CgalExceptionGuard& operator=(const CgalExceptionGuard&) = delete;
    CgalExceptionGuard(CgalExceptionGuard&&) = delete;
    CgalExceptionGuard& operator=(CgalExceptionGuard&&) = delete;

private:
    CGAL::Failure_behaviour prev_behaviour_;
};

} // namespace cgalsupport

#endif // CGAL_EXCEPTION_GUARD_H