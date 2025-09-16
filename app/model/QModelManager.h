#pragma once
#include "QAlgorithmSystemAdaptor.h"
#include "QModelIOSystemAdaptor.h"
#include <QObject>
#include <QUrl>
#include <memory>

namespace systems
{
	class SystemPluginManager;
}
namespace systems::io
{
	class ModelIOSystem;
}
class ModelManager;
class QModelObserver;

class QModelManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
public:
    explicit QModelManager(QObject* parent = nullptr);
    ~QModelManager();

    Q_INVOKABLE void removeModel(int id);
    Q_INVOKABLE QObject* getOperator(int id);
    ModelManager* getModelManager();
    QModelObserver* getModelObserver();
    systems::algo::QAlgorithmSystemAdaptor getAlgorithmSystemAdaptor();
    systems::io::QModelIOSystemAdaptor getModelIOSystemAdaptor();

signals:
    void modelAdded(int id);
    void modelRemoved(int id);
    void modelUpdated(int id);
    void modelNameChanged(int id, const QString& newName);
    void splineLoadFailed(const QString& message);

private:
    std::unique_ptr<ModelManager>  core_;
    std::unique_ptr<QModelObserver> observer_; 
    std::unique_ptr<systems::io::ModelIOSystem> io_system_;
    std::unique_ptr<systems::algo::AlgorithmSystem> algo_system_;
    std::unique_ptr<systems::SystemPluginManager> plugin_manager_;
};
