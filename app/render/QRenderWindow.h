/**
* @file：QRenderWindow.h
* @brief：定义渲染窗口，以及渲染窗口中的操作
* @author：付轩宇 email 982531420@qq.com

*/

#ifndef Q_RENDER_WINDOW_H
#define Q_RENDER_WINDOW_H
#include "Core.h"
#include "QModelQuery.h"
#include "QSelection.h"
#include <QQuickVTKItem.h>
#include <QVTKRenderWindowAdapter.h>

#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQml/qqmlregistration.h>
#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCameraOrientationWidget.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include "MeshActor.h" 

class MeshActor;
class SelectManager;
class SplineActorManager;
class MeshActorManager;
class QRenderWindowStyle;
class vtkDisplaySizedImplicitPlaneWidget;

struct QRenderWindow : QQuickVTKItem { // 结构体继承QQuickVTKItem
    Q_OBJECT
    Q_PROPERTY(QSelection* selectedIDs READ selectedIDs NOTIFY selectedChanged)
    Q_PROPERTY(QModelQuery* query MEMBER model_query_ WRITE setModelQuery REQUIRED)
    Q_PROPERTY(bool cur_edge_render READ getCurEdgeRender NOTIFY curEdgeRenderChanged)
    Q_PROPERTY(bool cur_vertex_render READ getCurVertexRender WRITE setCurVertexRender NOTIFY curVertexRenderChanged)
    QML_ELEMENT
public:
    QRenderWindow(); // 槽函数，改变边框重置相机
    ~QRenderWindow() override;

    struct Data : vtkObject { // 结构体继承vtkObject
        static Data* New();
        vtkTypeMacro(Data, vtkObject);

        vtkNew<vtkRenderer> renderer_;

        /*std::unordered_map<Index, std::unique_ptr<MeshActor>> models_;*/
        vtkNew<QRenderWindowStyle> style_;
        vtkSmartPointer<vtkCameraOrientationWidget> orientationWidget = vtkSmartPointer<vtkCameraOrientationWidget>::New();

        std::unique_ptr<MeshActorManager> mesh_actor_manager_;
        std::unique_ptr<SplineActorManager> spline_actor_manager_;

        vtkNew<vtkDisplaySizedImplicitPlaneWidget> plane_widget_;
    };

    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override;
    void destroyingVTK(vtkRenderWindow* renderWindow, vtkUserData userData) override;

    Q_INVOKABLE void resetCamera();

    QSelection* selectedIDs();
    void setModelQuery(QModelQuery* query);
    void setCurEdgeRender(bool edge_render);
    bool getCurEdgeRender();
    void setCurVertexRender(bool is_render);
    bool getCurVertexRender();

    bool getIsEdgeRender(Data& vtk, Index model_id);
    bool getIsVertexRender(Data& vtk, Index model_id);

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
    Q_INVOKABLE void setRenderMode(Index model_id, QString render_mode);

    /**
     * @brief 边渲染
     * @param select_mode
     */
    Q_INVOKABLE void setEdgeRender(Index model_id, bool is_render);

    /**
     * @brief 改变可见性
     * @param select_mode
     */
    Q_INVOKABLE void setVisibility(Index model_id, bool visibility);

    Q_INVOKABLE void onModelChanged(Index model_id);
    Q_INVOKABLE void deleteModel(Index mode_id);

    /**
     * @brief 对网格对象设置全局裁剪模式
     * @param on 是否开启
     */
    Q_INVOKABLE void setMeshClip(bool on);

    Q_SLOT void setClick();
    
    
    /**
     * @brief 设置属性渲染方式
     * 
     * 在控制台中调用示例：
     * myItem.setAttriMode("face_color_3", 0, 2, "")
     * myItem.setAttriMode("face_vectors_3", 3, 2, "")
     * myItem.setAttriMode("vertex_vector_3", 3, 0, "")
     * myItem.setAttriMode(" ", 2, 0, "E:/MeshProjects/Project_Harmonic/data/texture_checker.bmp")
     * myItem.setAttriMode("vertex_scalars", 1, 0, "")
     * myItem.cancelAttri()
     * 
     * @param mode 渲染方式 0:RGB 1:SCALAR 2:UV 3:VECTOR
     * @param type 属性类型 0:VERTEX 1:EDGE 2:FACE 3:SOLID
     * @param texturePath 贴图路径
     */
    Q_INVOKABLE void setAttriMode(QString attr_name, int mode, int type, QString texturePath);

    /**
     * @brief 取消属性渲染
     * 在控制台中调用示例：
     * myItem.cancelAttri()
     */
    Q_INVOKABLE void cancelAttri();
    /**
     * @brief 设置3D Glyph缩放比例
     * 在控制台中调用示例：
     * myItem.setGlyph3DScaleFactor(0.5)
     */
    Q_INVOKABLE void setGlyph3DScaleFactor( double scale);
    /**
     * @brief 设置标量映射范围
     * 在控制台中调用示例：
     * myItem.setScalarRange(2.0, 5.0)
     */
    Q_INVOKABLE void setScalarRange(double min, double max);
    /**
     * @brief 重置标量映射范围
     * 在控制台中调用示例：
     * myItem.resetScalarRange()
     */
    Q_INVOKABLE void resetScalarRange();

    bool event(QEvent* ev) override;

signals:
    void selectedChanged();
    void curEdgeRenderChanged();
    void curVertexRenderChanged();
    void clicked();

private:
    bool edge_render_ {};
    ModelRenderMode renderMode_ {};
    bool vertex_render_ {};
    SelectMode select_mode_ {};

    vtkNew<vtkCamera> _camera;

    std::unique_ptr<SelectManager> selectManager_;
    MeshActor* cur_actor_ {};
    Index cur_actor_id_;

    std::unique_ptr<QMouseEvent> _click;
    const Data* data_ {};

    QModelQuery* model_query_ {};
};
#endif // Q_RENDER_WINDOW_H
