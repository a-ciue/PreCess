#pragma once
#include "EventBus.h"
#include "QAlgorithmSystemAdaptor.h"
#include "QEditSystemAdaptor.h"
#include "QFeatureSystemAdaptor.h"
#include "QModelIOSystemAdaptor.h"
#include "QModelObserver.h"
#include "QModelQuery.h"
#include "QSystemPluginManager.h"
#include "QUndoStackAdaptor.h"
#include <memory>
#include <string>
#include <string_view>

namespace session {
class Session;
}
class ModelLayer;

class QModelManager : public QObject {
    Q_OBJECT
    QML_SINGLETON
    QML_ELEMENT
    Q_PROPERTY(systems::QSystemPluginManager* systemPluginManager READ getSystemPluginManager CONSTANT)
    Q_PROPERTY(QModelObserver* observer READ getModelObserver CONSTANT)
    Q_PROPERTY(QModelQuery* query READ getModelQuery CONSTANT)
    Q_PROPERTY(systems::algo::QAlgorithmSystemAdaptor* algorithmSystem READ getAlgorithmSystemAdaptor CONSTANT)
    Q_PROPERTY(systems::io::QModelIOSystemAdaptor* ioSystem READ getModelIOSystemAdaptor CONSTANT)
    Q_PROPERTY(systems::edit::QEditSystemAdaptor* editSystem READ getEditSystemAdaptor CONSTANT)
    Q_PROPERTY(systems::feature::QFeatureSystemAdaptor* featureSystem READ getFeatureSystemAdaptor CONSTANT)
    Q_PROPERTY(QUndoStackAdaptor* undoStack READ getUndoStackAdaptor CONSTANT)
public:
    explicit QModelManager(std::string_view argv0, QObject* parent = nullptr);
    ~QModelManager();

    Q_INVOKABLE void removeModel(int id);
    Q_INVOKABLE void removeComponent(int id);
    Q_INVOKABLE void removeMesh(int componentId);
    Q_INVOKABLE void removeGeometry(int componentId);
    ModelLayer* getModelManager();
    QModelObserver* getModelObserver() const;
    QModelQuery* getModelQuery() const;
    systems::algo::QAlgorithmSystemAdaptor* getAlgorithmSystemAdaptor() const;
    systems::edit::QEditSystemAdaptor* getEditSystemAdaptor() const;
    systems::io::QModelIOSystemAdaptor* getModelIOSystemAdaptor() const;
    systems::feature::QFeatureSystemAdaptor* getFeatureSystemAdaptor() const;
    systems::QSystemPluginManager* getSystemPluginManager() const;
    QUndoStackAdaptor* getUndoStackAdaptor() const;

    static std::string_view argv0; //> 命令行参数 argv[0]，用于插件加载等需要程序路径的场景，由 main 函数在程序启动时设置，被传入 ModelManager 构造函数以供其使用
    /**
     * @brief 创建 QModelManager 实例的静态工厂方法，供 QML 使用
     */
    static QModelManager* create(QQmlEngine*, QJSEngine*);

signals:
    void modelAdded(int id);
    void modelRemoved(int id);
    void modelUpdated(int id);
    void modelNameChanged(int id, const QString& newName);
    void geometryLoadFailed(const QString& message);

private:
    std::unique_ptr<session::Session> session_; //> 会话组合根（模型层/undo 栈/事件总线/四系统），析构先于 Qt 适配器（见 ~QModelManager）
    std::unique_ptr<QModelObserver> observer_;
    std::unique_ptr<QModelQuery> query_;
    core::EventBus::Subscription param_bridge_sub_; //> 参数变更桥接订阅（随成员析构自动退订）
    core::EventBus::Subscription scalar_attribute_display_bridge_sub_; //> 标量属性显示请求桥接订阅
    std::unique_ptr<systems::algo::QAlgorithmSystemAdaptor> algo_adaptor_;
    std::unique_ptr<systems::io::QModelIOSystemAdaptor> io_adaptor_;
    std::unique_ptr<systems::edit::QEditSystemAdaptor> edit_adaptor_;
    std::unique_ptr<systems::feature::QFeatureSystemAdaptor> feature_adaptor_;
    std::unique_ptr<QUndoStackAdaptor> undo_adaptor_;
    std::unique_ptr<systems::QSystemPluginManager> q_plugin_manager_;
};
