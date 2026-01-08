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
     * myItem.setAttriMode("face_pressure", 1, 2)
     * myItem.setAttriMode("face_color_3", 0, 2)
     * myItem.setAttriMode("face_vectors_3", 3, 2)
     * myItem.setAttriMode("vertex_vector_3", 3, 0)
     * myItem.setAttriMode("vertex_vector_3", 3, 0, "",0.5);
     * myItem.setAttriMode("vertex_uv_2", 2, 0, "E:/MeshProjects/Project_Harmonic/data/texture_checker.bmp")
     * myItem.setAttriMode("vertex_scalars", 1, 0)
     * myItem.setAttriMode("vertex_scalars", 1, 0,"",-1,[2,6])
     * 对于blow.vtk:
     * myItem.setAttriMode("displacement9_3", 3, 0)
     * myItem.setAttriMode("displacement9_3", 3, 0,"",0.5)
     * myItem.setAttriMode("thickness9", 1, 0)
     * myItem.setAttriMode("thickness9", 1, 0,"",-1,[0,2])
     * @param attr_name 属性名
     * @param mode 渲染方式 0:RGB 1:SCALAR 2:UV 3:VECTOR
     * @param type 属性类型 0:VERTEX 1:EDGE 2:FACE 3:SOLID
     * @param texturePath 贴图路径
     * @param glyphScale 箭头缩放比例
     * @param scalarRange 标量范围 (不给就是默认)
     */
    Q_INVOKABLE void setAttriMode(
        QString attr_name,
        int mode,
        int type,
        QString texture_path = "",
        double glyph_scale = -1,
        QVariant scalar_range = QVariant());
    /**
     * @brief 取消属性渲染
     * 在控制台中调用示例：
     * myItem.cancelAttri()
     */
    Q_INVOKABLE void cancelAttri();

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
