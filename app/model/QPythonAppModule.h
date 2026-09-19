#ifndef Q_PYTHON_APP_MODULE_H
#define Q_PYTHON_APP_MODULE_H

class QObject;

/**
 * @brief app 侧 Python 子模块注册（precess.app）：真/桩编入 pythonApp 静态库目标（恒存在，按可用性二选一，判断只在目标定义处），消费方（modelQml、
 * 测试）直接链接（业务代码无 #ifdef，"不可用"是运行时状态：桩实现注册无效果）
 *
 * 后续 app 侧 Python 函数统一收在 precess.app 子模块，在真实现内对模块句柄
 * 继续 def 即可（pybind11 全套类型转换器可用：任意参数个数/类型/返回值、
 * 多重重载、关键字参数与默认值）。
 */
namespace python::app {

/**
 * @brief 创建 precess.app 子模块并注册 app 侧函数（须在解释器就绪后、GUI 主线程调用）
 * @param owner 定时器 parent 与 GUI 线程归属（QPythonRuntime）
 */
void registerAppModule(QObject* owner);

/**
 * @brief 关停定时器表：停掉全部定时器并释放脚本回调，须在解释器终结前调用
 */
void shutdownAppModule();

}

#endif
