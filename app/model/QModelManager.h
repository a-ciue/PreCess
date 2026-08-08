#pragma once
#include "EventBus.h"
#include "QAlgorithmSystemAdaptor.h"
#include "QEditSystemAdaptor.h"
#include "QFeatureSystemAdaptor.h"
#include "QGeometryOperations.h"
#include "QModelIOSystemAdaptor.h"
#include "QModelObserver.h"
#include "QModelQuery.h"
#include "QSystemPluginManager.h"
#include <memory>
#include <string>
#include <string_view>

namespace core {
class EventBus;
}
namespace systems {
class SystemPluginManager;
}
namespace systems::io {
class ModelIOSystem;
}
namespace systems::edit {
class EditSystem;
}
namespace systems::feature {
class FeatureSystem;
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
    Q_PROPERTY(QGeometryOperations* geometry READ getGeometryOperations CONSTANT)
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
    QGeometryOperations* getGeometryOperations() const;

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
    std::unique_ptr<ModelLayer> core_;
    std::unique_ptr<QModelObserver> observer_;
    std::unique_ptr<QModelQuery> query_;
    std::unique_ptr<QGeometryOperations> geometry_operations_;
    std::unique_ptr<systems::io::ModelIOSystem> io_system_;
    std::unique_ptr<systems::algo::AlgorithmSystem> algo_system_;
    std::unique_ptr<systems::edit::EditSystem> edit_system_;
    std::unique_ptr<core::EventBus> event_bus_; //> 事件总线，声明在 feature_system_ 之前以保证其更晚析构
    core::EventBus::Subscription param_bridge_sub_; //> 参数变更桥接订阅（随成员析构自动退订）
    std::unique_ptr<systems::feature::FeatureSystem> feature_system_;
    std::unique_ptr<systems::algo::QAlgorithmSystemAdaptor> algo_adaptor_;
    std::unique_ptr<systems::io::QModelIOSystemAdaptor> io_adaptor_;
    std::unique_ptr<systems::edit::QEditSystemAdaptor> edit_adaptor_;
    std::unique_ptr<systems::feature::QFeatureSystemAdaptor> feature_adaptor_;
    std::unique_ptr<systems::QSystemPluginManager> q_plugin_manager_;
    std::unique_ptr<systems::SystemPluginManager> plugin_manager_;
};
