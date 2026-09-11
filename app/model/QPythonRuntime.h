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
namespace precess {
class Runtime;
}

/**
 * @brief app 内嵌 Python 运行环境的 QML 接入层：QPythonRuntime 仅是
 * python/precess::Runtime（无 Qt 解释器宿主）之上的 QObject 薄壳
 *
 * 职责限于：QML 属性/信号桥接（available/availableChanged）、GUI 线程断言、
 * 字符串编解码（QString ↔ UTF-8）与日志。解释器生命周期、precess 导入、
 * 活会话注入（precess.current）、控制台执行与输出捕获均在 precess::Runtime。
 *
 * 线程约定：Python 与 GUI 线程绑定，所有入口须在 GUI 线程调用（断言把关），
 * 渲染线程不得触碰 Python。
 *
 * Python 嵌入不可用（未定义 PRECESS_EMBED_PYTHON，如 wasm 构建）时编译
 * 降级实现，available 恒 false。
 *
 * @sa QModelManager::pythonRuntime
 * @sa precess::Runtime
 */
class QPythonRuntime : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("经 QModelManager.pythonRuntime 访问")
    Q_PROPERTY(bool available READ isAvailable NOTIFY availableChanged)
public:
    /**
     * @brief 构造接入层，仅组装宿主配置，不启动解释器（懒初始化）
     * @param session QModelManager 持有的会话组合根，须比本对象活得久
     * @param parent Qt 对象树父节点
     */
    explicit QPythonRuntime(session::Session* session, QObject* parent = nullptr);
    /**
     * @brief 析构：终结解释器（经宿主）并丢弃 Python 侧活会话引用
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

    std::unique_ptr<precess::Runtime> runtime_; //> 无 Qt 解释器宿主（python/）；未启用嵌入时为空
};

#endif
