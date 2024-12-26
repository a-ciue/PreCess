#ifndef MYVTKITEM_H
#define MYVTKITEM_H
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
    Q_PROPERTY(std::vector<int> selectedIDs READ selectedIDs NOTIFY selectedChanged)
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

        vtkNew<MouseInteractorHighLightFace> faceStyle;
        vtkNew<MouseInteractorHighLightEdge> edgeStyle;
        vtkNew<MouseInteractorHighLightActor> blockStyle;
        vtkNew<MouseInteractorHighLightActor> groupStyle;
        vtkInteractorStyleWithClick* styles[4] {};
    };

    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override;
    void destroyingVTK(vtkRenderWindow* renderWindow, vtkUserData userData) override;

    void resetCamera();
    //void dispatchChangedSource();

    std::vector<int> selectedIDs();

    Q_INVOKABLE void changeRenderer(QString renderMode);
    Q_INVOKABLE void bindStyle(QString function);
    Q_INVOKABLE void unbindStyle();
    Q_INVOKABLE void changeEdgeRender(QString renderMode, bool render);
    Q_INVOKABLE void onModelInited(const std::unordered_map<int, std::unique_ptr<Patch>>* patches,
        const std::unordered_map<int, std::unique_ptr<Block>>* blocks,
        const std::unordered_map<int, std::unique_ptr<Group>>* groups);
    Q_INVOKABLE void blocksMerged(const std::vector<int>& block_ids, int father_block, const std::unordered_set<int>& father_block_patches);
    Q_INVOKABLE void groupUpdated(int group_id, const std::unordered_set<int>& group_blocks);
    Q_INVOKABLE void groupMerged(const std::vector<int>& group_ids, int father_group, const std::unordered_set<int>& father_group_blocks);
    Q_INVOKABLE void patchUpdated(int patch_id, const std::vector<std::array<double, 3>>& points, const std::vector<std::array<int, 3>>& triangles);
    Q_INVOKABLE void blockUpdated(int block_id, const std::unordered_set<int>& block_patches);

    Q_SLOT void setClick();

    // Q_PROPERTY(QString file READ file WRITE setFile NOTIFY fileChanged)
    //QString source() const { return _source; }
    //void setSource(QString v);

    bool event(QEvent* ev) override;

signals:
    void selectedChanged();
    void clicked();
 
private:
    vtkNew<vtkCamera> _camera;
    vtkInteractorStyleWithClick* curStyle{};
    std::unique_ptr<ModelActor> actor;
    QScopedPointer<QMouseEvent> _click;
};
#endif // MYVTKITEM_H
