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
#include <vtkPoints.h>

class MeshActor;
class SelectManager;
class GeometryActorManager;
class MeshActorManager;
class QRenderWindowStyle;
class vtkDisplaySizedImplicitPlaneWidget;

struct QRenderWindow : QQuickVTKItem { // 结构体继承QQuickVTKItem
    Q_OBJECT
    Q_PROPERTY(QSelection* selectedIDs READ selectedIDs NOTIFY selectedChanged)
    Q_PROPERTY(QModelQuery* query MEMBER model_query_ WRITE setModelQuery REQUIRED)
    Q_PROPERTY(bool cur_edge_render READ getCurEdgeRender NOTIFY curEdgeRenderChanged)
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
        std::unique_ptr<GeometryActorManager> geometry_actor_manager_;

        vtkNew<vtkDisplaySizedImplicitPlaneWidget> plane_widget_;

        vtkNew<vtkPoints> global_points_;
    };

    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override;
    void destroyingVTK(vtkRenderWindow* renderWindow, vtkUserData userData) override;

    Q_INVOKABLE void resetCamera();

    QSelection* selectedIDs();
    void setModelQuery(QModelQuery* query);
    void setCurEdgeRender(bool edge_render);
    bool getCurEdgeRender();

    bool getIsEdgeRender(Data& vtk, Index component_id);

    /**
     * @brief 选择模型
     * @param select_mode
     */
    Q_INVOKABLE void setSelectComponent(Index component_id);

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
    Q_INVOKABLE void setComponentEdgeRender(Index component_id, bool is_render);

    /**
     * @brief 改变可见性
     * @param select_mode
     */
    Q_INVOKABLE void setVisibility(Index model_id, bool visibility);
    Q_INVOKABLE void setComponentVisibility(Index component_id, bool visibility);

    Q_INVOKABLE void onModelChanged(Index model_id);
    Q_INVOKABLE void onComponentChanged(Index component_id);

    Q_INVOKABLE void deleteModel(Index mode_id);
    Q_INVOKABLE void deleteComponent(Index component_id);

    void updateGlobalVtkPoints();

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
     * modelQuery.getModelAttriName(0)
     * myItem.setAttriMode("face_pressure_1", 1, {})
     * myItem.setAttriMode("face_color_3", 0, {})
     * myItem.setAttriMode("face_vectors_3", 3, {})
     * myItem.setAttriMode("vertex_vector_3", 3, { "glyph_scale": 0.5 })
     * myItem.setAttriMode("vertex_uv_2", 2, { "texture_path": "E:/MeshProjects/Project_Harmonic/data/texture_checker.bmp" })
     * myItem.setAttriMode("vertex_scalars_1", 1, {})
     * myItem.setAttriMode("vertex_scalars_1", 1, { "scalar_range": [2, 6] })
     * // blow.vtk 示例：
     * myItem.setAttriMode("displacement9_3", 3, {})
     * myItem.setAttriMode("displacement9_3", 3, { "glyph_scale": 0.5 })
     * myItem.setAttriMode("thickness9_1", 1, {})
     * myItem.setAttriMode("thickness9_1", 1, { "scalar_range": [0, 2] })
     * 
     * @param attr_name 属性名 后缀_3表示属性分量为3
     * @param mode 渲染方式 0:RGB 1:SCALAR 2:UV 3:VECTOR
     * @param args 其他参数（可选），如：
     *   "texture_path": 贴图路径（string）
     *   "glyph_scale": 箭头缩放比例（double）
     *   "scalar_range": 标量范围（长度为2的数组）
     */
    Q_INVOKABLE void setAttriMode(
        QString attr_name,
        int mode,
        QVariantMap args);
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
    void clicked();

private:
    bool edge_render_ {};
    ModelRenderMode renderMode_ {};
    SelectMode select_mode_ {};

    vtkNew<vtkCamera> _camera;

    std::unique_ptr<SelectManager> selectManager_;
    MeshActor* cur_actor_ {};
    Index cur_component_id_;

    std::unique_ptr<QMouseEvent> _click;
    const Data* data_ {};

    QModelQuery* model_query_ {};

    void updateGlobalVtkPointsImpl(Data* vtk);
};
#endif // Q_RENDER_WINDOW_H
