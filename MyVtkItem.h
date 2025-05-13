/**
* @file：MyVtkItem.h
* @brief：定义渲染窗口，以及渲染窗口中的操作
* @author：付轩宇 email 982531420@qq.com

*/


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

#include "Style.h"
#include "Selection.h"
#include "ModelActor.h"
#include "SelectManager.h"
#include "Core.h" 
#include "ModelQuery.h"

struct QRenderWindow : QQuickVTKItem {            //结构体继承QQuickVTKItem
    Q_OBJECT
    Q_PROPERTY(QSelection* selectedIDs READ selectedIDs NOTIFY selectedChanged)
    Q_PROPERTY(QModelQuery* query MEMBER model_query_ WRITE setModelQuery REQUIRED)
    QML_ELEMENT
public:
    QRenderWindow();                              //槽函数，改变边框重置相机

    struct Data : vtkObject {                 //结构体继承vtkObject
        static Data* New();
        vtkTypeMacro(Data, vtkObject);


        vtkNew<vtkRenderer> renderer_;

        std::unordered_map<Index, std::unique_ptr<ModelActor>> models_;
        vtkNew<QRenderWindowStyle> style_;


    };

    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override;
    void destroyingVTK(vtkRenderWindow* renderWindow, vtkUserData userData) override;

    Q_INVOKABLE void resetCamera();
    //void dispatchChangedSource();

    QSelection* selectedIDs();
    void setModelQuery(QModelQuery* query);

	/**
     * @brief 选择模型
     * @param select_mode 
     */
    Q_INVOKABLE void setSelectModel(Index model_id);

	 /**
     * @brief 改变选择模式
     * @param select_mode
     */
    Q_INVOKABLE void setSelectMode(QString select_mode);
   
    /**
     * @brief 清空Selection
     * @param select_mode
     */
    Q_INVOKABLE void clearSelection();


    /**
     * @brief 改变渲染模式
     * @param select_mode
     */
    Q_INVOKABLE void setRenderMode(QString render_mode);

    /**
     * @brief 边渲染
     * @param select_mode
     */
    Q_INVOKABLE void setEdgeRender(bool is_render);

    /**
     * @brief 改变可见性
     * @param select_mode
     */
    Q_INVOKABLE void setVisibility(Index model_id, bool visibility);


    Q_INVOKABLE void onModelChanged(Index model_id);
    Q_INVOKABLE void deleteModel(Index mode_id);

    Q_SLOT void setClick();

    // Q_PROPERTY(QString file READ file WRITE setFile NOTIFY fileChanged)
    //QString source() const { return _source; }
    //void setSource(QString v);

    bool event(QEvent* ev) override;

signals:
    void selectedChanged();
    void clicked();
 
private:
    bool edge_render_;
    RenderMode renderMode_{};
    SelectMode select_mode_ {};

    vtkNew<vtkCamera> _camera;
    
    std::unique_ptr<SelectManager> selectManager_;
    ModelActor* cur_actor_{};
    Index cur_actor_id_;

    std::unique_ptr<QMouseEvent> _click;
    const Data* data_{};
   
    QModelQuery* model_query_ {};
 };
#endif // MYVTKITEM_H
