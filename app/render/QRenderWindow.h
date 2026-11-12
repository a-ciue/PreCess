/**
* @file：QRenderWindow.h
* @brief：定义渲染窗口，以及渲染窗口中的操作
* @author：付轩宇 email 982531420@qq.com

*/

#ifndef Q_RENDER_WINDOW_H
#define Q_RENDER_WINDOW_H
#include "Core.h"
#include "MeshPipelineData.h"
#include "QModelQuery.h"
#include "QSelection.h"
#include "SurfacePrecision.h"
#include <QQuickVTKItem.h>
#include <QVTKRenderWindowAdapter.h>
#include <unordered_map>

#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQml/qqmlregistration.h>
#include <vtkActor.h>
#include <vtkAxisActor2D.h>
#include <vtkCameraOrientationWidget.h>
#include <vtkPoints.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>

#include <memory>

class MeshActor;
class SelectManager;
class InteractionService;
class GeometryActorManager;
class MeshActorManager;
class QRenderWindowStyle;
class vtkDisplaySizedImplicitPlaneWidget;
class IMeshIdQuery;
namespace systems::feature {
class QFeatureSystemAdaptor;
}

struct QRenderWindow : QQuickVTKItem { // 结构体继承QQuickVTKItem
    Q_OBJECT
    Q_PROPERTY(QSelection* selectedIDs READ selectedIDs NOTIFY selectedChanged)
    Q_PROPERTY(int selectionRevision READ selectionRevision WRITE setSelectionRevision NOTIFY selectionRevisionChanged)
    Q_PROPERTY(QModelQuery* query MEMBER model_query_ WRITE setModelQuery REQUIRED)
    Q_PROPERTY(int geometryStyle READ getGeometryStyle WRITE setGeometryStyle NOTIFY geometryStyleChanged)
    Q_PROPERTY(int surfacePrecision READ getSurfacePrecision WRITE setSurfacePrecision NOTIFY surfacePrecisionChanged)
    Q_PROPERTY(int meshStyle READ getMeshStyle WRITE setMeshStyle NOTIFY meshStyleChanged)
    QML_ELEMENT
public:
    QRenderWindow();
    ~QRenderWindow() override;

    struct Data : vtkObject { // 结构体继承vtkObject
        static Data* New();
        vtkTypeMacro(Data, vtkObject);

        vtkNew<vtkRenderer> renderer_;
        vtkNew<vtkRenderer> overlay_renderer_; //> 叠加层：测量文字标注置顶，不被模型遮挡

        /*std::unordered_map<Index, std::unique_ptr<MeshActor>> models_;*/
        vtkNew<QRenderWindowStyle> style_;
        unsigned long selection_observer_tag_ { }; //> 随当前场景图注册和移除
        vtkSmartPointer<vtkCameraOrientationWidget> orientationWidget = vtkSmartPointer<vtkCameraOrientationWidget>::New();

        std::unique_ptr<MeshActorManager> mesh_actor_manager_;
        std::unique_ptr<GeometryActorManager> geometry_actor_manager_;

        vtkNew<vtkDisplaySizedImplicitPlaneWidget> plane_widget_;
        std::uint64_t clip_revision_ { }; //> 当前渲染侧裁剪交互身份

        vtkNew<vtkAxisActor2D> scale_bar_axis_; //> 比例尺标尺轴（叠加层底部中央，段长与刻度随相机缩放联动更新）
    };

    vtkUserData initializeVTK(vtkRenderWindow* renderWindow) override;
    void destroyingVTK(vtkRenderWindow* renderWindow, vtkUserData userData) override;

    Q_INVOKABLE void resetCamera();

    QSelection* selectedIDs();
    //! @brief GUI 侧选择参数版本，防止旧渲染结果写入新参数。
    int selectionRevision() const { return selection_revision_; }
    void setSelectionRevision(int revision);
    void setModelQuery(QModelQuery* query);

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
     * @brief 设置面选择的角度扩散参数
     * @param enabled 是否启用按角度扩散
     * @param angle_deg 相邻面法向夹角阈值，单位为度
     */
    Q_INVOKABLE void setFaceSelectionByAngle(bool enabled, double angle_deg);

    /**
     * @brief 投递选择与高亮清空命令，GUI 缓存仅在收到渲染侧空快照后更新
     */
    Q_INVOKABLE void clearSelection();

    /**
     * @brief 注入功能系统适配器（交互服务经 FeatureSystem::activeInteraction 获取激活的交互状态）
     * @param adaptor QModelManager.featureSystem
     */
    Q_INVOKABLE void setFeatureAdaptor(QObject* adaptor);

    /**
     * @brief 显示/隐藏比例尺（测量插件激活时启用，随相机缩放自动更新刻度）
     * @param on 是否显示
     */
    Q_INVOKABLE void setScaleBarVisible(bool on);

    /**
     * @brief 改变可见性
     * @param select_mode
     */
    Q_INVOKABLE void setVisibility(Index model_id, bool visibility);
    Q_INVOKABLE void setComponentVisibility(Index component_id, bool visibility);
    /** @brief 设置网格显隐；显示时恢复人工隐藏面和面隔离，保留几何及裁剪。 */
    Q_INVOKABLE void setMeshVisibility(Index component_id, bool visibility);
    Q_INVOKABLE void setGeometryVisibility(Index component_id, bool visibility);

    Q_INVOKABLE void setGeometryStyle(int style);
    int getGeometryStyle();

    /** @brief 设置曲面显示精细度：0 粗糙、1 标准（当前默认）、2 精细、3 非常精细。 */
    Q_INVOKABLE void setSurfacePrecision(int precision);
    int getSurfacePrecision() const;

    Q_INVOKABLE void setMeshStyle(int style);
    int getMeshStyle();

    /**
     * @brief 启用或停用一种网格拓扑诊断类别
     * @param category 类别序号，依次为边界边、边界面、非流形边、非流形点、孤立边、孤立点、二面角边
     * @param enabled 是否启用
     */
    Q_INVOKABLE void setTopologyDiagnosticCategoryEnabled(int category, bool enabled);

    /** @brief 设置二面角诊断边的角度范围，单位为度 */
    Q_INVOKABLE void setDihedralAngleRange(double minimum, double maximum);

    /**
     * @brief 启用或停用一种几何拓扑诊断类别。
     * @param category 类别序号，依次为边界边、孤立边、非流形边、细小边、细小面、重复面、
     *        自相交、几何干涉、无效拓扑。
     * @param enabled 是否启用。
     */
    Q_INVOKABLE void setGeometryTopologyDiagnosticCategoryEnabled(int category, bool enabled);

    /** @brief 设置几何拓扑诊断的细小边长度阈值。 */
    Q_INVOKABLE void setGeometryTopologyDiagnosticSmallEdgeLength(double threshold);
    /** @brief 设置几何拓扑诊断的细小面面积阈值。 */
    Q_INVOKABLE void setGeometryTopologyDiagnosticSmallFaceArea(double threshold);

    /** @brief 捕获自持网格，继承整体开关和裁剪，清理旧选择后提交组件快照。 */
    void loadMeshData(Index component_id, const MeshDataVtk& data);

    Q_INVOKABLE void onModelChanged(Index model_id);
    Q_INVOKABLE void onComponentChanged(Index component_id);

    Q_INVOKABLE void deleteModel(Index mode_id);
    Q_INVOKABLE void deleteComponent(Index component_id);

    /**
     * @brief 对网格对象设置全局裁剪模式
     * @param on 是否开启
     */
    Q_INVOKABLE void setMeshClip(bool on);

    /**
     * @brief 隐藏面选择器当前高亮选中的面（右键菜单"隐藏"）
     *
     * 仅面选择模式有效，其余模式为空操作；隐藏为渲染层单元过滤，
     * 不修改模型数据，被隐藏面不渲染也不可拾取；安装结果时清空选择和高亮。
     */
    Q_INVOKABLE void hideSelectedFaces();

    /** @brief 显示面选择器选中的原始面，保留其余面和几何的显隐。 */
    Q_INVOKABLE void showSelectedFaces();
    /** @brief 仅在所选面所属组件内隐藏其他面，保留其他组件、几何及同组件其他对象的显示。 */
    Q_INVOKABLE void isolateSelectedFaces();

    /** @brief 恢复全部组件的网格和几何，清除人工隐藏并结束面隔离，保留裁剪。 */
    Q_INVOKABLE void showAll();

    /**
     * @brief 反转目标组件；几何沿用组件整体开关，网格兼容局部隐藏面
     * @param component_id 目标组件，局部隐藏面的反转规则与场景反转一致
     */
    Q_INVOKABLE void reverseComponentVisibility(Index component_id);
    /** @brief 反转 GUI 记录的整个网格和几何场景，不依赖对象树提供组件列表。 */
    Q_INVOKABLE void reverseSceneVisibility();

    Q_SLOT void setClick();

    /**
     * @brief 设置属性渲染；标量按点、边、面、体顺序使用首个同名数组。
     *
     * 控制台可直接调用同一入口：
     * App.registry.renderWindow.setAttriMode(0, "f_pressure_1", 1, { scalar_range: [0, 2], display_range: [0.5, 1.5] })
     * 省略 scalar_range 使用默认映射，省略 display_range 关闭筛选；空列表同样表示省略。
     * App.registry.renderWindow.setAttriMode(0, "f_color_3", 0, {})
     * App.registry.renderWindow.setAttriMode(0, "v_vector_3", 3, { glyph_scale: 0.5 })
     * App.registry.renderWindow.setAttriMode(0, "v_uv_2", 2, { texture_path: "E:/data/texture_checker.bmp" })
     * 与面板共享标量偏好和显示意图时，使用 App.registry.scalarRanges.showAttribute(0, "f_pressure_1")。
     * 直接调用窗口只更新当前显示，不改写界面历史偏好。
     *
     * @param component_id 目标组件 ID
     * @param attr_name 属性名称；前缀只用于显示，不参与实体域选择
     * @param mode 渲染方式 0:RGB 1:SCALAR 2:UV 3:VECTOR
     * @param args 策略参数；标量范围必须是两个有序有限数值，其他参数沿用原策略
     * @return 标量配置已接受或其他策略请求已排队，不表示渲染已经完成
     */
    Q_INVOKABLE bool setAttriMode(
        Index component_id,
        QString attr_name,
        int mode,
        QVariantMap args);
    /**
     * @brief 取消指定 Component 的属性渲染
     * 在控制台中调用示例：
     * App.registry.scalarRanges.cancel(App.selection.activeComponentId)
     * @param component_id 目标 Component ID
     */
    Q_INVOKABLE void cancelComponentAttri(Index component_id);

    /** @brief 查询完整原始值域的两个端点；没有有限单值值域时返回空列表。 */
    Q_INVOKABLE QVariantList scalarRangeInfo(Index component_id, const QString& attribute);

    bool event(QEvent* ev) override;

signals:
    void selectedChanged();
    void selectionRevisionChanged();
    void geometryStyleChanged();
    void surfacePrecisionChanged();
    void meshStyleChanged();
    void clicked();
    void rightClicked();
    /** @brief 原始属性数据变化，通知界面重新查询元信息。 */
    void scalarRangeChanged();
    /** @brief 在 GUI 决定显隐后同步通知对象树，渲染线程只安装结果。 */
    void componentVisibilityChanged(Index component_id, bool mesh_visible, bool geometry_visible);

protected:
    void itemChange(ItemChange change, const ItemChangeData& value) override;

private:
    /**
     * @brief 发布完成的网格结果；输出重新生成时清理选择，标量配置解除时取消着色。
     * @param activate_scalar_bar 主动应用时切换色柱；自动恢复只刷新原归属色柱。
     */
    void publishMeshUpdate(Index component_id, MeshSnapshotUpdate update, bool activate_scalar_bar = false);
    /** @brief 解除当前标量配置和筛选，恢复非范围隐藏结果；界面自行保留偏好。 */
    void deactivateScalarRange(Index component_id);
    // 只记录共享色柱的归属，用于场景图重建时恢复同一属性的现有色柱。
    Index scalar_bar_component_id_ { -1 };

    //! @brief GUI 线程保存 DPR，并将叠加层像素尺寸更新排入渲染队列。
    void updateOverlayScale(qreal ratio);
    qreal overlay_device_pixel_ratio_ { 1.0 };

    /** @brief GUI 根据已发布选择执行面显隐，结果经渲染队列单向安装。 */
    enum class FaceOperation { Hide,
        Show,
        Isolate,
        ShowScene,
        Reverse };
    using SelectedFaces = std::unordered_map<Index, std::vector<vtkIdType>>;
    using MeshGenerations = std::unordered_map<Index, std::uint64_t>;
    using GeometryVisibility = std::unordered_map<Index, bool>;
    /** @brief 直接使用 GUI 缓存的选择和网格代次计算显隐结果。 */
    void applyFaceOperation(FaceOperation operation, const std::vector<Index>& ids = { });
    /** @brief 查询 GUI 记录的有效网格与几何组件并集，供场景反转确定范围。 */
    std::vector<Index> getSceneComponentIds() const;
    /** @brief 发布当前组件结果，缓存用于场景图重建。 */
    std::shared_ptr<const MeshDataSnapshot> prepareMeshSnapshot(Index component_id);
    /** @brief 将裁剪控件值排队交给 GUI，并丢弃停用前的旧交互。 */
    void publishClipPlane(const MeshClipPlane& plane, std::uint64_t revision);
    /** @brief 在 GUI 线程更新所有主网格的隐藏/裁剪数据。 */
    void applyClipPlane(const MeshClipPlane& plane);
    std::unordered_map<Index, std::unique_ptr<MeshPipelineData>> mesh_pipeline_data_; //> GUI 独占可变管线
    std::unordered_map<Index, std::shared_ptr<const MeshDataSnapshot>> mesh_snapshots_; //> 发布后只读
    GeometryVisibility geometry_visibility_; //> 仅含已存在几何的 GUI 整体显隐状态
    MeshClipPlane mesh_clip_;
    std::uint64_t clip_revision_ { };

    //! @brief 渲染线程制作选择快照，排队交付 GUI 线程，禁止 QML 直接读取拾取器。
    void publishSelection();
    //! @brief 只应用网格/几何显隐，不触发回传；由组件级操作合并后统一通知。
    void applyMeshVisibility(Index component_id, bool visibility);
    void applyGeometryVisibility(Index component_id, bool visibility);
    /** @brief 在 GUI 线程发布当前显隐结果，对象树只接收最新状态。 */
    void publishComponentVisibility(Index component_id);
    // QQuickVTKItem 会接管事件分发，GUI 排队通知交给独立 QObject。
    QObject selection_dispatcher_;
    std::shared_ptr<const Selection> selection_snapshot_; //> 仅 GUI 线程访问
    int selection_revision_ { }; //> GUI 侧版本
    int render_selection_revision_ { }; //> 渲染侧已应用版本
    SelectedFaces selected_faces_; //> 与 QML 选择同批回传的原始面 ID，仅 GUI 访问
    MeshGenerations selected_face_generations_; //> 各选中组件的源网格代次，仅 GUI 访问

    GeometryRenderStyle geometry_style_ { GeometryRenderStyle::SurfaceWithEdges };
    int surface_precision_ { surface_precision::kDefault }; //> GUI 侧请求档位，场景重建时恢复
    MeshRenderStyle mesh_style_ { MeshRenderStyle::FaceWithEdges };

    std::unique_ptr<SelectManager> select_manager_;
    std::unique_ptr<InteractionService> interaction_service_;
    systems::feature::QFeatureSystemAdaptor* feature_adaptor_ { };
    MeshActor* cur_actor_ { };
    Index cur_component_id_ { -1 };

    std::unique_ptr<QMouseEvent> _click;
    const Data* data_ { };

    QModelQuery* model_query_ { };

    std::unique_ptr<IMeshIdQuery> mesh_id_query_; //> IMeshIdQuery 桥接实现，随 setModelQuery 注入

    //! @brief 注入渲染刷新回调到 FeatureSystem（initializeVTK 与 setFeatureAdaptor 各调一次，确保初始化顺序无关）
    void injectRenderRefreshCallback();
};
#endif // Q_RENDER_WINDOW_H
