#ifndef MODELMANAGER_H
#define MODELMANAGER_H
#include "Model.h"
#include "MyVtkItem.h"
#include <qqmlregistration.h>
#include <QQmlContext>
#include <QObject>


class ModelManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(MyVtkItem* vtkItem READ vtkItem WRITE setVtkItem)
    

public:
    //构造函数
    explicit ModelManager(QObject* parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE Model* model(const QString& modelName) {
        auto it = models_.find(modelName);
        if (it != models_.end()) {
            return it->second.get();
        }
        return nullptr; // 如果找不到模型，返回空指针
    }

    MyVtkItem* vtkItem();
    void setVtkItem(MyVtkItem* item);

    //多模型管理功能
    void addModel(const QString& modelName, std::unique_ptr<Model> model);
    Q_INVOKABLE void removeModel(const QString& modelName);
    Q_INVOKABLE Model* getModel(const QString& modelName) const;

    Q_INVOKABLE void readSpline(QUrl spline_path);
    Q_INVOKABLE void readMesh(QUrl target_mesh);
    Q_INVOKABLE void writeMesh(const QString& modelName, QUrl target_mesh, QString renderMode, QString extension);

    Q_INVOKABLE void reName(const QString& oldName, const QString& newName);


signals:
    void splineLoadFailed(QString message);
    void modelNameChanged(const QString& oldName, const QString& newName);
    void modelAdded(const QString& modelName);
    void modelRemoved(const QString& modelName);


private:
    //std::unique_ptr<Model> model_;
    //使用unordered_map替代原unique_ptr用于满足存储多模型的要求
    std::unordered_map<QString, std::unique_ptr<Model>> models_; 
    MyVtkItem* vtk_item_{};
};
#endif // MODELMANAGER_H
