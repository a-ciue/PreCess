#pragma once
#include "QAlgorithmSystemAdaptor.h"
#include "QEditSystemAdaptor.h"
#include "QModelIOSystemAdaptor.h"
#include "QSystemPluginManager.h"
#include <memory>

namespace systems {
class SystemPluginManager;
}
namespace systems::io {
class ModelIOSystem;
}
namespace systems::edit {
class EditSystem;
}
class ModelLayer;
class QModelObserver;

class QModelManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(systems::QSystemPluginManager* systemPluginManager READ getSystemPluginManager)
public:
    explicit QModelManager(std::string_view argv0, QObject* parent = nullptr);
    ~QModelManager();

    Q_INVOKABLE void removeModel(int id);
    Q_INVOKABLE void removeComponent(int id);
    Q_INVOKABLE QObject* getOperator(int id);
    ModelLayer* getModelManager();
    QModelObserver* getModelObserver();
    systems::algo::QAlgorithmSystemAdaptor getAlgorithmSystemAdaptor();
    systems::edit::QEditSystemAdaptor getEditSystemAdaptor();
    systems::io::QModelIOSystemAdaptor getModelIOSystemAdaptor();
    systems::QSystemPluginManager* getSystemPluginManager();

signals:
    void modelAdded(int id);
    void modelRemoved(int id);
    void modelUpdated(int id);
    void modelNameChanged(int id, const QString& newName);
    void geometryLoadFailed(const QString& message);

private:
    std::unique_ptr<ModelLayer> core_;
    std::unique_ptr<QModelObserver> observer_;
    std::unique_ptr<systems::io::ModelIOSystem> io_system_;
    std::unique_ptr<systems::algo::AlgorithmSystem> algo_system_;
    std::unique_ptr<systems::edit::EditSystem> edit_system_;
    std::unique_ptr<systems::QSystemPluginManager> q_plugin_manager_;
    std::unique_ptr<systems::SystemPluginManager> plugin_manager_;
};
