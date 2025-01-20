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
    Model* model() {
        return model_.get();
    }
    MyVtkItem* vtkItem();
    void setVtkItem(MyVtkItem* item);

    Q_INVOKABLE void readSpline(QUrl spline_path, double size);
    Q_INVOKABLE void readMesh(QUrl target_mesh);
    Q_INVOKABLE void writeMesh(QUrl target_mesh, QString renderMode, QString extension);

signals:
    void splineLoadFailed(QString message);

private:
    std::unique_ptr<Model> model_;
    MyVtkItem* vtk_item_{};

    void connectVtk();
};
#endif // MODELMANAGER_H
