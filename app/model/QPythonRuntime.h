#ifndef Q_PYTHON_RUNTIME_H
#define Q_PYTHON_RUNTIME_H

#include <QObject>
#include <QString>
#include <QVariantMap>

#include <memory>

namespace session {
class Session;
}
namespace python {
class Runtime;
}

/**
 * @brief app 内嵌 Python 运行环境的 QML 接入层：QPythonRuntime 仅是
 * python::Runtime（无 Qt 解释器宿主）之上的 QObject 薄壳
 *
 * 编入 pythonApp 静态库目标；QML 类型（原 QML_ELEMENT/UNCREATABLE 语义）经
 * modelQml 的 QPythonRuntimeQml.h 外来包装注册，类本身不携带 QML 宏。
 *
 * 职责限于：QML 属性/信号桥接（available/availableChanged）、GUI 线程断言、
 * 字符串编解码（QString ↔ UTF-8）与日志。解释器就绪后经 python_app
 * （QPythonAppModule，app 侧 pybind11 注册逻辑）创建 app 专属子模块 precess.app
 * 并注册任意签名本机函数——app 侧 Python 函数统一收在该模块，定时策略与
 * 生命周期亦由其管理。解释器生命周期、precess 模块导入、活会话注入
 * （precess.current）、控制台执行与输出捕获均在 python::Runtime。
 *
 * 线程约定：Python 与 GUI 线程绑定，所有入口须在 GUI 线程调用（断言把关），
 * 渲染线程不得触碰 Python。
 *
 * 本类无编译期降级分支：precess_runtime 目标恒存在，Python 嵌入不可用
 * （缺 Python3/pybind11、PRECESS_BUILD_PYTHON=OFF 或 wasm）时宿主为桩实现，
 * available 恒 false、lastError() 说明原因，模块注册亦为桩空操作。
 *
 * @sa QModelManager::pythonRuntime
 * @sa python::Runtime
 * @sa python_app::registerAppModule
 */
class QPythonRuntime : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
public:
    /**
     * @brief 构造接入层，仅组装宿主配置，不启动解释器（懒初始化）
     * @param session QModelManager 持有的会话组合根，须比本对象活得久
     * @param parent Qt 对象树父节点
     */
    explicit QPythonRuntime(session::Session* session, QObject* parent = nullptr);
    /**
     * @brief 析构：先关停 precess.app 定时器表（释放脚本回调），再终结解释器（经宿主）
     */
    ~QPythonRuntime() override;

    /**
     * @brief 解释器是否就绪（初始化成功且 precess 模块可用）
     */
    bool isAvailable() const;
    /**
     * @brief 最近一次初始化失败的描述（界面提示用），空串表示无失败记录
     */
    Q_INVOKABLE QString lastError() const;

public slots:
    /**
     * @brief 初始化解释器并导入 precess 模块（幂等；失败后不重试）
     */
    void initialize();
    /**
     * @brief 执行一段控制台源码（与交互式解释器同判定规则）
     * @param source 待执行源码，可为多行未完块（未完时只做编译判定、不执行）
     * @return ok 是否已执行且成功；incomplete 源码是否未完（等待续行）；
     *         output stdout/stderr 捕获；error 异常描述（traceback 全文）
     */
    QVariantMap execute(const QString& source);
    /**
     * @brief 解释器版本串（sys.version），不可用时返回空串
     */
    QString version() const;

signals:
    /**
     * @brief 可用状态变化（初始化成功或失败后发出）
     */
    void availableChanged();

private:
    void ensureInitialized(); //> execute 的懒初始化入口，仅未初始化且无失败记录时才真正初始化

    std::unique_ptr<python::Runtime> runtime_; //> 无 Qt 解释器宿主（python/）；未启用嵌入时为空
};

#endif
