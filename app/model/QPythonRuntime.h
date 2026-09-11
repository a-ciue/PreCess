#ifndef Q_PYTHON_RUNTIME_H
#define Q_PYTHON_RUNTIME_H

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtQml/qqml.h>
#include <memory>

namespace session {
class Session;
}

/**
 * @brief app 内嵌 Python 运行环境：解释器生命周期管理与 Python 控制台执行入口
 *
 * 线程约定（重要）：Python 与 GUI 线程绑定，所有入口（initialize/execute/version）
 * 必须在 GUI 线程调用，渲染线程不得触碰 Python。GIL 自解释器初始化起归本线程
 * （GUI 线程）所有，pybind11 的 gil 守卫在同线程为无操作配对，保留它以约束未来
 * 可能的跨线程扩展。
 *
 * 初始化为幂等懒式（首次 initialize 或 execute 触发）：经 PyConfig 把标准库根
 * 固定为 CMake 期绑定的解释器目录（PRECESS_PYTHON_HOME），向 sys.path 注入
 * precess 绑定模块目录后导入，并把 QModelManager 持有的活会话以引用策略注入
 * precess.current——Python 侧不持有其所有权，生命周期由 ~QModelManager 保证
 * （先终结解释器、再析构会话）。Python/pybind11 不可用（未定义
 * PRECESS_EMBED_PYTHON，如 wasm 构建）时整类降级为不可用态，available 恒 false。
 *
 * @sa QModelManager::pythonRuntime
 */
class QPythonRuntime : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("经 QModelManager.pythonRuntime 访问")
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
public:
    /**
     * @brief 构造运行时，仅记录活会话指针，不启动解释器（懒初始化）
     * @param session QModelManager 持有的会话组合根，须比本对象活得久
     * @param parent Qt 对象树父节点
     */
    explicit QPythonRuntime(session::Session* session, QObject* parent = nullptr);
    /**
     * @brief 析构：丢弃 Python 侧活会话引用并终结解释器
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

    struct State; //> PIMPL：隔离 pybind11/CPython 头文件，避免污染 app/model 其他编译单元
    std::unique_ptr<State> state_; //> 解释器与模块句柄（仅 PRECESS_EMBED_PYTHON 下有实际内容）
};

#endif
