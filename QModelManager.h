#pragma once
#include "systems/io/ModelIOSystem.h"
#include "systems/io/OBJModelHandler.h"
#include <QObject>
#include <QUrl>
#include <memory>
#include "ModelManager.h"
#include "ModelImporter.h"
#include "ModelObserver.h"

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

signals:
    void modelAdded(int id);
    void modelRemoved(int id);
    void modelUpdated(int id);
    void modelNameChanged(int id, const QString& newName);
    void splineLoadFailed(const QString& message);

private:
    std::unique_ptr<ModelManager>  core_;
    std::unique_ptr<ModelImporter> importer_;
    std::unique_ptr<QModelObserver> observer_; 
    std::unique_ptr<systems::io::ModelIOSystem> io_system_;
};
