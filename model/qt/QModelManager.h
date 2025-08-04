#pragma once
#include "ModelIOSystem.h"
#include "SystemPluginManager.h"
#include "QAlgorithmSystemAdaptor.h"
#include "AlgorithmSystem.h"
#include <QObject>
#include <QUrl>
#include <memory>
#include "ModelManager.h"
#include "QModelObserver.h"

class QModelManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
public:
    explicit QModelManager(QObject* parent = nullptr);

    Q_INVOKABLE void importModel(const QUrl& url);
    Q_INVOKABLE void removeModel(int id);
    Q_INVOKABLE QObject* getOperator(int id);
    ModelManager* getModelManager();
    QModelObserver* getModelObserver();
    systems::algo::QAlgorithmSystemAdaptor getAlgorithmSystemAdaptor();

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
