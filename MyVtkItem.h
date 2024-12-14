#include <QQuickVTKItem.h>
#include <QVTKRenderWindowAdapter.h>

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCapsuleSource.h>
#include <vtkConeSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkRendererCollection.h>

#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQml/qqmlregistration.h>
#include <vtkSphereSource.h>
#include <vtkOBJReader.h>

#include "Model.h"
#include "Style.h"

struct MyVtkItem : QQuickVTKItem {            //结构体继承QQuickVTKItem
    Q_OBJECT
    //Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
    QML_ELEMENT
public:
    MyVtkItem();                              //槽函数，改变边框重置相机

    struct Data : vtkObject {                 //结构体继承vtkObject
        static Data* New();
        vtkTypeMacro(Data, vtkObject);

        //vtkNew<vtkActor> actor;               //以下是用模板构建各种图形（或者控制等）的类
        // 0 face_renderer, 1 block_renderer, 2 group_renderer
        vtkNew<vtkRenderer> renderer[3];
        vtkRenderer* curRenderer {};

        std::unique_ptr<Model> model;
        vtkNew<MouseInteractorHighLightFace> faceStyle;
        vtkNew<MouseInteractorHighLightEdge> edgeStyle;
        vtkNew<MouseInteractorHighLightActor> blockStyle;
        vtkNew<MouseInteractorHighLightActor> groupStyle;
        vtkInteractorStyleWithClick* styles[4] {};
        vtkInteractorStyleWithClick* curStyle{};
    };

    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override;
    void destroyingVTK(vtkRenderWindow* renderWindow, vtkUserData userData) override;

    void resetCamera();
    //void dispatchChangedSource();

    Q_INVOKABLE void readSpline(QUrl spline_path);
    Q_INVOKABLE void writeMesh(QUrl target_mesh, QString renderMode, QString extension);
    Q_INVOKABLE void changeRenderer(QString renderMode);
    Q_INVOKABLE void bindStyle(QString function);
    Q_INVOKABLE void unbindStyle();
    Q_INVOKABLE void changeEdgeRender(QString renderMode, bool render);
    //Q_INVOKABLE void commitChange(QString function);
    Q_INVOKABLE void commitBlockMerge();
    Q_INVOKABLE void commitBlockRemesh();
    Q_INVOKABLE void commitGroupMerge();
    Q_INVOKABLE void commitGroupRemesh();
    Q_INVOKABLE void commitFaceCut();
    Q_INVOKABLE void commitEdgeCut();

    Q_SLOT void setClick();

    // Q_PROPERTY(QString file READ file WRITE setFile NOTIFY fileChanged)
    //QString source() const { return _source; }
    //void setSource(QString v);

    bool event(QEvent* ev) override;

signals:
    //void sourceChanged(QString);
    void splineLoadFailed(QString);
    void clicked();

private:
    QString _source;
    vtkNew<vtkCamera> _camera;
    QScopedPointer<QMouseEvent> _click;
};
