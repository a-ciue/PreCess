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
    Q_PROPERTY(Model* model READ model)

public:
    //构造函数
    explicit ModelManager(QObject* parent = nullptr) : QObject(parent) {}

    Model* model(const QString& modelName) {
        auto it = models_.find(modelName);
        if (it != models_.end()) {
            return it->second.get();
        }
        return nullptr; // 如果找不到模型，返回空指针
    }

    MyVtkItem* vtkItem();
    void setVtkItem(const QString& modelName, MyVtkItem* item);

    //多模型管理功能
    Q_INVOKABLE void addModel(const QString& modelName, std::unique_ptr<Model> model);
    Q_INVOKABLE void removeModel(const QString& modelName);
    Q_INVOKABLE Model* getModel(const QString& modelName) const;

    Q_INVOKABLE void readSpline(const QString& modelName, QUrl spline_path);
    Q_INVOKABLE void readMesh(const QString& modelName, QUrl target_mesh);
    Q_INVOKABLE void writeMesh(const QString& modelName, QUrl target_mesh, QString renderMode, QString extension);

signals:
    void splineLoadFailed(QString message);

private:
    //std::unique_ptr<Model> model_;
    //使用unordered_map替代原unique_ptr用于满足存储多模型的要求
    std::unordered_map<QString, std::unique_ptr<Model>> models_; 
    MyVtkItem* vtk_item_{};

    void connectVtk(const QString& modelName);
};
#endif // MODELMANAGER_H
