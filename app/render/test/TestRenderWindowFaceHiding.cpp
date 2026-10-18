/**
 * @file TestRenderWindowFaceHiding.cpp
 * @brief 面隐藏与恢复时选择快照、高亮及面身份的视口回归测试
 */
#include "ComponentData.h"
#include "GeometryActor.h"
#include "GeometryActorManager.h"
#include "GeometryData.h"
#include "MakeMeshData.h"
#include "MakeMeshDataVtk.h"
#include "MeshActorManager.h"
#include "MeshActorSelectOp.h"
#include "ModelLayer.h"
#include "QRenderWindow.h"
#include "QRenderWindowStyle.h"
#include "SessionQuery.h"

#include <BRep_Builder.hxx>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QThread>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Vertex.hxx>
#include <catch2/catch_test_macros.hpp>
#include <gp_Pnt.hxx>
#include <vtkActorCollection.h>
#include <vtkCamera.h>
#include <vtkCellData.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkIdTypeArray.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindowInteractor.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace {

//! @brief 创建 OpenGL Qt Quick 环境，串行消费渲染调度与 GUI 快照通知。
QGuiApplication& testApplication()
{
    // Qt 在 Windows 上默认写入调试器，转交 stderr 使离屏环境诊断进入测试日志。
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString& message) {
        std::fprintf(stderr, "%s\n", qPrintable(message));
    });
    // Windows 的 offscreen 平台不提供 OpenGL 上下文，使用原生平台创建不可见测试窗口。
#ifdef Q_OS_WIN
    qputenv("QT_QPA_PLATFORM", "windows");
#else
    qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    qputenv("QT_QUICK_BACKEND", "rhi");
    qputenv("QSG_RHI_BACKEND", "opengl");
    if (qEnvironmentVariableIsEmpty("QSG_RENDER_LOOP"))
        qputenv("QSG_RENDER_LOOP", "basic");
    QQuickVTKItem::setGraphicsApi();
    static int argc = 1;
    static char app_name[] = "TestRenderWindowFaceHiding";
    static char* argv[] = { app_name, nullptr };
    static QGuiApplication app(argc, argv);
    return app;
}

//! @brief 两个不重叠的面共用真实视口，覆盖公共菜单入口的异步调度与拾取。
struct FaceHidingViewport {
    MeshData mesh;
    ModelLayer model;
    session::SessionQuery native_query { model };
    QModelQuery query { &native_query };
    QQuickWindow window;
    QRenderWindow item;

    //! @brief 加载原面 0、1；隐藏面 0 后，面 1 的渲染单元索引会压缩为 0。
    //! @param show_window 是否立即显示窗口；false 用于验证首次场景初始化前的命令。
    explicit FaceHidingViewport(bool show_window = true)
    {
        // 复用已验证的演示网格体数据，将测试面放在体和边之外，避免拾取遮挡。
        mesh = MakeMeshData();
        const Index first_point = static_cast<Index>(mesh.vertex_positions_.size());
        mesh.vertex_positions_.insert(mesh.vertex_positions_.end(), { { 5.0, 0.0, 0.0 }, { 6.0, 0.0, 0.0 }, { 6.0, 1.0, 0.0 }, { 5.0, 1.0, 0.0 }, { 7.0, 0.0, 0.0 }, { 8.0, 0.0, 0.0 }, { 8.0, 1.0, 0.0 }, { 7.0, 1.0, 0.0 } });
        mesh.vertex_count_ = static_cast<Index>(mesh.vertex_positions_.size());
        mesh.face_vertices_ = { first_point, first_point + 1, first_point + 2, first_point + 3,
            first_point + 4, first_point + 5, first_point + 6, first_point + 7 };
        mesh.face_vertices_offset_ = { 0, 4, 8 };
        window.resize(600, 600);
        window.setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
        window.setOpacity(0.0);
        window.setPersistentSceneGraph(false);
        window.setPersistentGraphics(false);
        item.setParentItem(window.contentItem());
        item.setSize(QSizeF(600, 600));
        item.loadMeshData(7, MakeMeshDataVtk(mesh));
        item.dispatch_async([](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
            auto* data = QRenderWindow::Data::SafeDownCast(user_data);
            auto* camera = data->renderer_->GetActiveCamera();
            camera->SetPosition(6.5, 0.5, 5.0);
            camera->SetFocalPoint(6.5, 0.5, 0.0);
            camera->SetViewUp(0.0, 1.0, 0.0);
            camera->SetParallelScale(2.0);
        });
        item.setSelectMode("Face");
        if (show_window)
            window.show();
    }

    //! @brief 建立真实模型查询并加载几何，覆盖 GUI 状态初始化和渲染安装路径。
    void loadGeometry(Index component_id, bool publish = true)
    {
        if (!query.hasComponent(component_id)) {
            ComponentDatas components;
            for (Index id = 0; id <= 10; ++id) {
                auto component = std::make_unique<ComponentData>();
                if (id == 7)
                    component->mesh = mesh.clone();
                components.push_back(std::move(component));
            }
            model.addModel("geometry fixture", std::move(components));
            item.setModelQuery(&query);
        }
        BRep_Builder builder;
        TopoDS_Compound shape;
        TopoDS_Vertex vertex;
        builder.MakeCompound(shape);
        builder.MakeVertex(vertex, gp_Pnt(100.0, 100.0, 0.0), 1.0e-7);
        builder.Add(shape, vertex);
        auto geometry = std::make_unique<GeometryData>();
        geometry->setRootShape(shape);
        model.findComponent(component_id)->geometry = std::move(geometry);
        if (publish)
            item.onComponentChanged(component_id);
    }

    //! @brief 在视口对象析构前释放场景图，确保 VTK 服务按原有生命周期清理。
    ~FaceHidingViewport()
    {
        window.hide();
        window.releaseResources();
        QCoreApplication::processEvents();
    }

    //! @brief 通过多轮渲染屏障消费选择导出、GUI 计算、渲染安装及最终 GUI 通知。
    bool flush()
    {
        QElapsedTimer timer;
        timer.start();
        // 每轮屏障后消费 GUI 队列，再为 GUI 新发布的渲染结果排入下一轮屏障。
        for (int round = 0; round < 8; ++round) {
            auto completed = std::make_shared<std::atomic_bool>(false);
            item.dispatch_async([completed](vtkRenderWindow*, QQuickVTKItem::vtkUserData) {
                completed->store(true);
            });
            while (!completed->load() && timer.elapsed() < 5000) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                QThread::msleep(1);
            }
            if (!completed->load())
                return false;
            QCoreApplication::processEvents();
        }
        return true;
    }

    //! @brief 通过真实点击选中指定原模型面，并由 style 事件发布选择结果。
    void selectFace(Index face_id, double offset_y = 0.0)
    {
        item.dispatch_async([face_id, offset_y](vtkRenderWindow* render_window, QQuickVTKItem::vtkUserData user_data) {
            auto* data = QRenderWindow::Data::SafeDownCast(user_data);
            data->renderer_->SetWorldPoint(face_id == 0 ? 5.5 : 7.5, 0.5 + offset_y, 0.0, 1.0);
            data->renderer_->WorldToDisplay();
            double display[3] { };
            data->renderer_->GetDisplayPoint(display);
            render_window->GetInteractor()->SetEventPosition(
                static_cast<int>(std::lround(display[0])), static_cast<int>(std::lround(display[1])));
            data->style_->SetClick();
            data->style_->OnLeftButtonUp();
        });
        REQUIRE(flush());
    }

    //! @brief 读取 GUI 独立快照，确认菜单及选择参数看到的原模型面集合。
    std::vector<Index> selectedFaceIds()
    {
        std::unique_ptr<QSelection> selection(item.selectedIDs());
        return selection ? selection->get()->ids : std::vector<Index> { };
    }

    //! @brief 读取当前可见的独立高亮面数，确认隐藏选择不会留下红色覆盖层。
    vtkIdType highlightedCellCount()
    {
        vtkIdType count = 0;
        item.dispatch_async([&count](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
            auto* data = QRenderWindow::Data::SafeDownCast(user_data);
            auto* actors = data->renderer_->GetActors();
            actors->InitTraversal();
            while (auto* actor = actors->GetNextActor()) {
                if (actor->GetPickable() || !actor->GetVisibility())
                    continue;
                auto* mapper = vtkCompositePolyDataMapper::SafeDownCast(actor->GetMapper());
                auto* partitions = mapper ? vtkPartitionedDataSet::SafeDownCast(mapper->GetInputDataObject(0, 0)) : nullptr;
                if (!partitions)
                    continue;
                for (unsigned int i = 0; i < partitions->GetNumberOfPartitions(); ++i) {
                    if (auto* poly = vtkPolyData::SafeDownCast(partitions->GetPartition(i)))
                        count += poly->GetNumberOfCells();
                }
            }
        });
        REQUIRE(flush());
        return count;
    }

    //! @brief 在渲染调度中读取面过滤管线，避免 GUI 直接访问 VTK 状态。
    std::vector<Index> visibleFaceIds(Index component_id = 7)
    {
        std::vector<Index> ids;
        // 回调只记录对象是否有效，断言留在测试线程，避免异常穿过 Qt 事件回调。
        bool has_face_actor = false;
        bool has_face_mapper = false;
        item.dispatch_async([component_id, &ids, &has_face_actor, &has_face_mapper](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
            auto* data = QRenderWindow::Data::SafeDownCast(user_data);
            MeshActorSelectOp select_op(data->mesh_actor_manager_->getComponentActor(component_id));
            auto* actor = vtkActor::SafeDownCast(&select_op.getFaceActor());
            has_face_actor = actor != nullptr;
            if (!has_face_actor)
                return;
            auto* mapper = vtkPolyDataMapper::SafeDownCast(actor->GetMapper());
            has_face_mapper = mapper != nullptr;
            if (!has_face_mapper)
                return;
            mapper->Update();
            auto* face_ids = vtkIdTypeArray::SafeDownCast(
                mapper->GetInput()->GetCellData()->GetArray("PrecessFaceIds"));
            if (face_ids) {
                for (vtkIdType i = 0; i < face_ids->GetNumberOfTuples(); ++i)
                    ids.push_back(static_cast<Index>(face_ids->GetValue(i)));
            }
        });
        REQUIRE(flush());
        INFO("测试组件的面渲染对象必须为 vtkActor，映射器必须为 vtkPolyDataMapper");
        REQUIRE(has_face_actor);
        REQUIRE(has_face_mapper);
        return ids;
    }
};

} // namespace

TEST_CASE("QRenderWindow hiding faces retains selection for showing the same faces")
{
    testApplication();
    FaceHidingViewport env;
    REQUIRE(env.flush());
    env.selectFace(0);
    REQUIRE(env.selectedFaceIds() == std::vector<Index>({ 0 }));
    REQUIRE(env.highlightedCellCount() == 1);

    int updates = 0;
    QObject::connect(&env.item, &QRenderWindow::selectedChanged, &env.item, [&updates] { ++updates; });
    env.item.hideSelectedFaces();
    REQUIRE(env.flush());

    CHECK(updates == 1);
    CHECK(env.selectedFaceIds() == std::vector<Index>({ 0 }));
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 1 }));
    CHECK(env.highlightedCellCount() == 0);
    std::unique_ptr<QSelection> retained(env.item.selectedIDs());
    REQUIRE(retained);
    CHECK(retained->get()->component_id == 7);
    CHECK(retained->get()->type == ElementEnum::Face);

    // 连续隐藏仍使用同一原始面 ID；随后“显示”应恢复这批隐藏面。
    env.item.hideSelectedFaces();
    env.item.hideSelectedFaces();
    REQUIRE(env.flush());
    CHECK(env.selectedFaceIds() == std::vector<Index>({ 0 }));
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 1 }));
    SECTION("show retained hidden faces")
    {
        env.item.showSelectedFaces();
        REQUIRE(env.flush());
        CHECK(env.visibleFaceIds() == std::vector<Index>({ 0, 1 }));
    }
    SECTION("isolate retained hidden faces")
    {
        env.item.isolateSelectedFaces();
        REQUIRE(env.flush());
        CHECK(env.visibleFaceIds() == std::vector<Index>({ 0 }));
    }
}

TEST_CASE("QRenderWindow restores retained hidden face selections in their own components")
{
    testApplication();
    std::unique_ptr<MeshData> second_mesh;
    FaceHidingViewport env;
    REQUIRE(env.flush());
    second_mesh = env.mesh.clone();
    for (auto& point : second_mesh->vertex_positions_)
        point[1] += 1.5;
    env.item.loadMeshData(8, MakeMeshDataVtk(*second_mesh));
    REQUIRE(env.flush());
    env.selectFace(0);
    env.selectFace(1, 1.5);
    REQUIRE(env.selectedFaceIds().size() == 2);
    env.item.hideSelectedFaces();
    REQUIRE(env.flush());
    CHECK(env.selectedFaceIds().size() == 2);
    CHECK(env.highlightedCellCount() == 0);
    CHECK(env.visibleFaceIds(7) == std::vector<Index>({ 1 }));
    CHECK(env.visibleFaceIds(8) == std::vector<Index>({ 0 }));
    // 恢复分别使用各组件自己的原面 ID，不能将不同组件的选择合成一张面表。
    env.item.showSelectedFaces();
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds(7) == std::vector<Index>({ 0, 1 }));
    CHECK(env.visibleFaceIds(8) == std::vector<Index>({ 0, 1 }));
}

TEST_CASE("QRenderWindow retained hidden faces do not alias newly picked render indices")
{
    testApplication();
    FaceHidingViewport env;
    REQUIRE(env.flush());
    env.selectFace(0);
    env.item.hideSelectedFaces();
    REQUIRE(env.flush());

    // 面 1 的渲染下标压缩为 0，不能与保留的原面 0 当作同一选择。
    env.selectFace(1);
    CHECK(env.selectedFaceIds() == std::vector<Index>({ 0, 1 }));
    CHECK(env.highlightedCellCount() == 1);
    env.selectFace(1);
    CHECK(env.selectedFaceIds() == std::vector<Index>({ 0 }));
    CHECK(env.highlightedCellCount() == 0);
    env.selectFace(1);
    env.item.hideSelectedFaces();
    REQUIRE(env.flush());
    CHECK(env.selectedFaceIds() == std::vector<Index>({ 0, 1 }));
    CHECK(env.visibleFaceIds().empty());
    CHECK(env.highlightedCellCount() == 0);
    env.item.showSelectedFaces();
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 0, 1 }));
}

TEST_CASE("QRenderWindow restoring faces invalidates compressed selection indices")
{
    testApplication();
    FaceHidingViewport env;
    REQUIRE(env.flush());
    env.selectFace(0);
    REQUIRE(env.selectedFaceIds() == std::vector<Index>({ 0 }));
    env.item.hideSelectedFaces();
    REQUIRE(env.flush());

    // 主动清除保留的隐藏面，再选择渲染下标已压缩为 0 的原面 1。
    env.item.clearSelection();
    env.selectFace(1);
    REQUIRE(env.selectedFaceIds() == std::vector<Index>({ 1 }));
    env.item.showAllHiddenFaces(7);
    REQUIRE(env.flush());
    CHECK(env.selectedFaceIds().empty());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 0, 1 }));

    // 没有新选择时再次隐藏应为空操作；重新拾取后仍按原模型面身份隐藏。
    env.item.hideSelectedFaces();
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 0, 1 }));
    env.selectFace(1);
    REQUIRE(env.selectedFaceIds() == std::vector<Index>({ 1 }));
    env.item.hideSelectedFaces();
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 0 }));
}

TEST_CASE("QRenderWindow reverses hidden faces independently of the selection mode")
{
    testApplication();
    FaceHidingViewport env;
    REQUIRE(env.flush());
    env.selectFace(0);
    env.item.hideSelectedFaces();
    REQUIRE(env.flush());
    REQUIRE(env.visibleFaceIds() == std::vector<Index>({ 1 }));

    QString mode;
    SECTION("Face") { mode = "Face"; }
    SECTION("Component") { mode = "Component"; }
    SECTION("None") { mode = "None"; }
    env.item.setSelectMode(mode);
    env.selectFace(1);
    env.item.reverseComponentVisibility(7);
    REQUIRE(env.flush());
    CHECK(env.selectedFaceIds().empty());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 0 }));

    // 隐藏面不能拾取；重新显示的面仍返回原模型面 id。
    env.item.setSelectMode("Face");
    env.selectFace(1);
    CHECK(env.selectedFaceIds().empty());
    env.selectFace(0);
    CHECK(env.selectedFaceIds() == std::vector<Index>({ 0 }));
    env.item.reverseComponentVisibility(7);
    REQUIRE(env.flush());
    CHECK(env.selectedFaceIds().empty());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 1 }));

    // 不等待对象树回传即连续点击，仍应逐次基于最新显隐意图取反。
    env.item.reverseComponentVisibility(7);
    env.item.reverseComponentVisibility(7);
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 1 }));
    env.item.showAllHiddenFaces(7);
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 0, 1 }));
}

TEST_CASE("QRenderWindow reversing visibility handles partially visible and hidden components together")
{
    testApplication();
    FaceHidingViewport env;
    REQUIRE(env.flush());
    env.selectFace(0);
    env.item.hideSelectedFaces();
    auto second_mesh = env.mesh.clone();
    for (auto& point : second_mesh->vertex_positions_)
        point[1] += 1.5;
    env.item.loadMeshData(8, MakeMeshDataVtk(*second_mesh));
    REQUIRE(env.flush());
    env.selectFace(0, 1.5);
    env.item.hideSelectedFaces();
    REQUIRE(env.flush());
    env.item.setMeshVisibility(8, false);
    env.item.loadMeshData(9, MakeMeshDataVtk(env.mesh));
    REQUIRE(env.flush());
    std::map<Index, std::pair<bool, bool>> visibility;
    QObject::connect(&env.item, &QRenderWindow::componentVisibilityChanged, &env.item,
        [&visibility](Index component_id, bool mesh_visible, bool geometry_visible) {
            visibility[component_id] = { mesh_visible, geometry_visible };
        });
    env.item.reverseSceneVisibility();
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds(7) == std::vector<Index>({ 0 }));
    CHECK(env.visibleFaceIds(8) == std::vector<Index>({ 0, 1 }));
    CHECK(visibility[7] == std::make_pair(true, false));
    CHECK(visibility[8] == std::make_pair(true, false));
    CHECK(visibility[9] == std::make_pair(false, false));
}

TEST_CASE("QRenderWindow reversing geometry follows the original component visibility switch")
{
    testApplication();
    FaceHidingViewport env;
    REQUIRE(env.flush());
    bool mesh_enabled = true;
    bool geometry_enabled = false;
    bool hidden_style = false;
    SECTION("visible mesh and hidden geometry") { }
    SECTION("hidden mesh and visible geometry")
    {
        mesh_enabled = false;
        geometry_enabled = true;
    }
    SECTION("both visible") { geometry_enabled = true; }
    SECTION("both hidden") { mesh_enabled = false; }
    SECTION("hidden render style preserves the component visibility switch") { hidden_style = true; }
    env.loadGeometry(7);
    env.item.setMeshVisibility(7, mesh_enabled);
    env.item.setGeometryVisibility(7, geometry_enabled);
    env.item.dispatch_async([hidden_style](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        if (hidden_style) {
            data->mesh_actor_manager_->getComponentActor(7)->setRenderStyle(MeshRenderStyle::Hidden);
            data->geometry_actor_manager_->getComponentActor(7)->setRenderStyle(GeometryRenderStyle::Hidden);
        }
    });
    REQUIRE(env.flush());
    std::pair<bool, bool> visibility { mesh_enabled, geometry_enabled };
    QObject::connect(&env.item, &QRenderWindow::componentVisibilityChanged, &env.item,
        [&visibility](Index, bool mesh_visible, bool geometry_visible) {
            visibility = { mesh_visible, geometry_visible };
        });
    const bool expected = !(mesh_enabled || geometry_enabled);
    env.item.reverseComponentVisibility(7);
    REQUIRE(env.flush());
    CHECK(visibility == std::make_pair(expected, expected));
    std::pair<bool, bool> actor_visibility;
    env.item.dispatch_async([&actor_visibility](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        actor_visibility = { data->mesh_actor_manager_->getComponentActor(7)->isVisibilityEnabled(),
            data->geometry_actor_manager_->getComponentActor(7)->isVisibilityEnabled() };
    });
    REQUIRE(env.flush());
    CHECK(actor_visibility == visibility);
    // 不等待对象树回传连续反转两次，仍应回到同一组件开关状态。
    env.item.reverseComponentVisibility(7);
    env.item.reverseComponentVisibility(7);
    REQUIRE(env.flush());
    CHECK(visibility == std::make_pair(expected, expected));
}

TEST_CASE("QRenderWindow showing selected faces restores them through a hidden parent")
{
    testApplication();
    FaceHidingViewport env;
    REQUIRE(env.flush());
    env.selectFace(1);
    env.item.setMeshVisibility(7, false);
    REQUIRE(env.flush());
    REQUIRE(env.selectedFaceIds() == std::vector<Index>({ 1 }));
    std::pair<bool, bool> visibility { false, false };
    QObject::connect(&env.item, &QRenderWindow::componentVisibilityChanged, &env.item,
        [&visibility](Index, bool mesh_visible, bool geometry_visible) {
            visibility = { mesh_visible, geometry_visible };
        });
    env.item.showSelectedFaces();
    REQUIRE(env.flush());
    CHECK(env.selectedFaceIds().empty());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 1 }));
    CHECK(visibility == std::make_pair(true, false));
    env.selectFace(0);
    CHECK(env.selectedFaceIds().empty());
    env.selectFace(1);
    CHECK(env.selectedFaceIds() == std::vector<Index>({ 1 }));
}

TEST_CASE("QRenderWindow isolating faces keeps selections from multiple components and hides geometry")
{
    testApplication();
    std::unique_ptr<MeshData> second_mesh;
    FaceHidingViewport env;
    REQUIRE(env.flush());
    env.loadGeometry(7);
    env.loadGeometry(10);
    REQUIRE(env.flush());
    second_mesh = env.mesh.clone();
    for (auto& point : second_mesh->vertex_positions_)
        point[1] += 1.5;
    env.item.loadMeshData(8, MakeMeshDataVtk(*second_mesh));
    auto unselected_mesh = env.mesh.clone();
    for (auto& point : unselected_mesh->vertex_positions_)
        point[0] += 20.0;
    env.item.loadMeshData(9, MakeMeshDataVtk(*unselected_mesh));
    REQUIRE(env.flush());
    env.selectFace(1, 1.5);
    env.item.hideSelectedFaces();
    env.item.clearSelection();
    env.selectFace(0);
    env.selectFace(0, 1.5);
    REQUIRE(env.selectedFaceIds().size() == 2);
    std::map<Index, std::pair<bool, bool>> visibility;
    QObject::connect(&env.item, &QRenderWindow::componentVisibilityChanged, &env.item,
        [&visibility](Index component_id, bool mesh_visible, bool geometry_visible) {
            visibility[component_id] = { mesh_visible, geometry_visible };
        });
    env.item.isolateSelectedFaces();
    REQUIRE(env.flush());
    CHECK(env.selectedFaceIds().empty());
    CHECK(env.visibleFaceIds(7) == std::vector<Index>({ 0 }));
    CHECK(env.visibleFaceIds(8) == std::vector<Index>({ 0 }));
    CHECK(visibility[7] == std::make_pair(true, false));
    CHECK(visibility[8] == std::make_pair(true, false));
    CHECK(visibility[9] == std::make_pair(false, false));
    CHECK(visibility[10] == std::make_pair(false, false));
    bool others_hidden = false;
    env.item.dispatch_async([&others_hidden](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        others_hidden = !data->mesh_actor_manager_->getComponentActor(9)->isVisible()
            && !data->geometry_actor_manager_->getComponentActor(10)->isVisible();
    });
    REQUIRE(env.flush());
    CHECK(others_hidden);
    env.selectFace(1);
    CHECK(env.selectedFaceIds().empty());
    env.selectFace(0);
    CHECK(env.selectedFaceIds() == std::vector<Index>({ 0 }));
    // 面隔离后仍能使用全局反转，所选面的补集和其余组件分别恢复。
    env.item.reverseSceneVisibility();
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds(7) == std::vector<Index>({ 1 }));
    CHECK(env.visibleFaceIds(8) == std::vector<Index>({ 1 }));
    CHECK(visibility[7] == std::make_pair(true, false));
    CHECK(visibility[9] == std::make_pair(true, false));
    CHECK(visibility[10] == std::make_pair(false, true));
    env.item.showAll();
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds(7) == std::vector<Index>({ 0, 1 }));
    CHECK(env.visibleFaceIds(8) == std::vector<Index>({ 0, 1 }));
}

TEST_CASE("QRenderWindow face operations preserve non-face or empty selections")
{
    testApplication();
    FaceHidingViewport env;
    REQUIRE(env.flush());
    SECTION("empty face selection") { }
    SECTION("component selection")
    {
        env.item.setSelectMode("Component");
        env.selectFace(0);
        REQUIRE(env.selectedFaceIds() == std::vector<Index>({ 7 }));
    }
    std::unique_ptr<QSelection> before(env.item.selectedIDs());
    int updates = 0;
    QObject::connect(&env.item, &QRenderWindow::selectedChanged, &env.item, [&updates] { ++updates; });
    env.item.hideSelectedFaces();
    env.item.showSelectedFaces();
    env.item.isolateSelectedFaces();
    REQUIRE(env.flush());
    std::unique_ptr<QSelection> after(env.item.selectedIDs());
    REQUIRE(static_cast<bool>(after) == static_cast<bool>(before));
    if (before) {
        CHECK(after->get()->type == before->get()->type);
        CHECK(after->get()->ids == before->get()->ids);
        CHECK(after->get()->component_id == before->get()->component_id);
    }
    CHECK(updates == 0);
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 0, 1 }));
}

//! @brief 使用真实的独立渲染线程验证 GUI 数据管线、裁剪回传和选择交接。
TEST_CASE("QRenderWindow GUI mesh pipeline works with a separate render thread")
{
    qputenv("QSG_RENDER_LOOP", "threaded");
    testApplication();
    FaceHidingViewport env;
    REQUIRE(env.flush());
    std::atomic_bool separate_thread { false };
    auto* gui_thread = QThread::currentThread();
    env.item.dispatch_async([gui_thread, &separate_thread](vtkRenderWindow*, QQuickVTKItem::vtkUserData) {
        separate_thread.store(QThread::currentThread() != gui_thread);
    });
    REQUIRE(env.flush());
    REQUIRE(separate_thread.load());
    env.selectFace(0);
    REQUIRE(env.selectedFaceIds() == std::vector<Index>({ 0 }));
    env.item.hideSelectedFaces();
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 1 }));
    CHECK(env.selectedFaceIds() == std::vector<Index>({ 0 }));
    env.item.setMeshClip(true);
    REQUIRE(env.flush());
    env.item.setMeshClip(false);
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 1 }));
    env.item.showSelectedFaces();
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 0, 1 }));
    CHECK(env.selectedFaceIds().empty());
}

//! @brief 连续提交隐藏、恢复和整体开关时，结果必须按调用顺序生效。
TEST_CASE("QRenderWindow applies consecutive face commands in call order")
{
    testApplication();
    FaceHidingViewport env;
    REQUIRE(env.flush());
    env.selectFace(0);
    env.item.hideSelectedFaces();
    env.item.showSelectedFaces();
    env.item.clearSelection();
    env.item.setMeshVisibility(7, true);
    REQUIRE(env.flush());
    bool visible = false;
    env.item.dispatch_async([&visible](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        visible = data->mesh_actor_manager_->getComponentActor(7)->isVisibilityEnabled();
    });
    REQUIRE(env.flush());
    CHECK(visible);
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 0, 1 }));
}

//! @brief GUI 已完成面命令后释放真实场景图，重建后按原顺序安装快照和整体开关。
TEST_CASE("QRenderWindow submitted face operations survive scene graph recreation")
{
    qputenv("QSG_RENDER_LOOP", "threaded");
    testApplication();
    // 原子标志先于视口构造，确保析构阶段的场景图信号也不会访问已销毁的捕获对象。
    std::atomic_bool invalidated { false };
    std::atomic_bool recreated { false };
    std::atomic_bool exported { false };
    FaceHidingViewport env;
    REQUIRE(env.flush());
    env.selectFace(0);
    env.item.hideSelectedFaces();
    REQUIRE(env.flush());
    REQUIRE(env.visibleFaceIds() == std::vector<Index>({ 1 }));
    QObject::connect(&env.window, &QQuickWindow::sceneGraphInvalidated, &env.window, [&invalidated] { invalidated.store(true); }, Qt::DirectConnection);
    QObject::connect(&env.window, &QQuickWindow::sceneGraphInitialized, &env.window, [&recreated] { recreated.store(true); }, Qt::DirectConnection);

    // GUI 同步完成面数据计算，渲染安装尚未进入下一次同步，随后关闭窗口。
    env.item.showAllHiddenFaces(7);
    env.item.setMeshVisibility(7, false);
    env.item.dispatch_async([&exported](vtkRenderWindow*, QQuickVTKItem::vtkUserData) { exported.store(true); });
    env.window.hide();
    env.window.releaseResources();
    QElapsedTimer timer;
    timer.start();
    while (!invalidated.load() && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    REQUIRE(invalidated.load());
    REQUIRE_FALSE(exported.load());

    // 新场景重放缓存与已有渲染命令后，必须保留最新整体开关和已恢复的面集。
    env.window.show();
    REQUIRE(env.flush());
    REQUIRE(recreated.load());
    REQUIRE(exported.load());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 0, 1 }));
    bool visible = true;
    env.item.dispatch_async([&visible](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        visible = data->mesh_actor_manager_->getComponentActor(7)->isVisibilityEnabled();
    });
    REQUIRE(env.flush());
    CHECK_FALSE(visible);

    // 再提交一次反转，验证场景重建后仍可继续处理新操作。
    env.item.reverseComponentVisibility(7);
    REQUIRE(env.flush());
    env.item.dispatch_async([&visible](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        visible = data->mesh_actor_manager_->getComponentActor(7)->isVisibilityEnabled();
    });
    REQUIRE(env.flush());
    CHECK(visible);
}

//! @brief 对象树先收到反转再主动显示时，不允许渲染完成后的旧通知覆盖新图标。
TEST_CASE("QRenderWindow publishes GUI visibility decisions without stale render notifications")
{
    testApplication();
    bool tree_visible = true;
    FaceHidingViewport env;
    REQUIRE(env.flush());
    QObject::connect(&env.item, &QRenderWindow::componentVisibilityChanged, &env.item,
        [&tree_visible](Index id, bool mesh_visible, bool geometry_visible) {
            if (id == 7)
                tree_visible = mesh_visible || geometry_visible;
        });
    env.item.reverseComponentVisibility(7);
    CHECK_FALSE(tree_visible); // GUI 决定后立即同步，不等待渲染往返。
    tree_visible = true; // 模拟对象树显示按钮的即时反馈。
    env.item.setMeshVisibility(7, true);
    CHECK(tree_visible);
    REQUIRE(env.flush());
    bool actor_visible = false;
    env.item.dispatch_async([&actor_visible](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        actor_visible = data->mesh_actor_manager_->getComponentActor(7)->isVisibilityEnabled();
    });
    REQUIRE(env.flush());
    CHECK(actor_visible);
    CHECK(tree_visible);
}

//! @brief GUI 等待渲染侧空快照；同上下文允许旧选择先到达，参数切换和网格换代仍拒绝旧结果。
TEST_CASE("QRenderWindow clears GUI selection only after render snapshot delivery")
{
    testApplication();
    QObject gui_dispatcher;
    FaceHidingViewport env;
    REQUIRE(env.flush());
    env.selectFace(0);
    env.item.hideSelectedFaces();
    REQUIRE(env.flush());
    REQUIRE(env.selectedFaceIds() == std::vector<Index>({ 0 }));
    enum class Change { Clear,
        Mode,
        Parameter,
        Reload };
    Change change = Change::Clear;
    SECTION("explicit clearing") { }
    SECTION("selection mode changes") { change = Change::Mode; }
    SECTION("parameter context changes") { change = Change::Parameter; }
    SECTION("mesh generation changes") { change = Change::Reload; }
    bool retained_before_delivery = false;
    bool requested = false;
    std::vector<std::vector<Index>> received;
    QObject::connect(&env.item, &QRenderWindow::selectedChanged, &gui_dispatcher, [&] {
        if (requested)
            received.push_back(env.selectedFaceIds());
    });
    env.item.dispatch_async([&](vtkRenderWindow* render_window, QQuickVTKItem::vtkUserData user_data) {
        // GUI 先收到清理请求，随后收到清理前的 {0,1}；真正的空快照要等下一次渲染队列执行。
        QMetaObject::invokeMethod(&gui_dispatcher, [&] {
            if (change == Change::Clear)
                env.item.clearSelection();
            else if (change == Change::Mode)
                env.item.setSelectMode("Component");
            else if (change == Change::Parameter)
                env.item.setSelectionRevision(env.item.selectionRevision() + 1);
            else
                env.item.loadMeshData(7, MakeMeshDataVtk(env.mesh));
            retained_before_delivery = env.selectedFaceIds() == std::vector<Index>({ 0 });
            requested = true; }, Qt::QueuedConnection);
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        data->renderer_->SetWorldPoint(7.5, 0.5, 0.0, 1.0);
        data->renderer_->WorldToDisplay();
        double display[3] { };
        data->renderer_->GetDisplayPoint(display);
        render_window->GetInteractor()->SetEventPosition(
            static_cast<int>(std::lround(display[0])), static_cast<int>(std::lround(display[1])));
        data->style_->SetClick();
        data->style_->OnLeftButtonUp();
    });
    REQUIRE(env.flush());
    REQUIRE(requested);
    CHECK(retained_before_delivery);
    REQUIRE_FALSE(received.empty());
    CHECK(received.back().empty());
    const auto old_selection = std::find(received.begin(), received.end(), std::vector<Index>({ 0, 1 }));
    CHECK((old_selection != received.end()) == (change == Change::Clear || change == Change::Mode));
    CHECK(env.selectedFaceIds().empty());
    CHECK(env.highlightedCellCount() == 0);
    // 清理后的空缓存不能再恢复刚才隐藏的面；只有重载会恢复完整网格。
    env.item.showSelectedFaces();
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds() == (change == Change::Reload ? std::vector<Index> { 0, 1 } : std::vector<Index> { 1 }));
}

//! @brief 几何状态由模型加载初始化，重载保留开关，移除后不能在全局反转中复活。
TEST_CASE("QRenderWindow maintains GUI geometry visibility across load update and removal")
{
    testApplication();
    std::map<Index, std::pair<bool, bool>> visibility;
    FaceHidingViewport env;
    // 真实几何已存在但尚未安装时，提前设置的开关也必须在首次加载后保留。
    env.loadGeometry(10, false);
    env.item.setGeometryVisibility(10, false);
    env.item.onComponentChanged(10);
    REQUIRE(env.flush());
    bool initially_hidden = false;
    env.item.dispatch_async([&initially_hidden](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        auto actor = data->geometry_actor_manager_->getComponentActor(10);
        initially_hidden = actor && !actor->isVisibilityEnabled();
    });
    REQUIRE(env.flush());
    REQUIRE(initially_hidden);
    QObject::connect(&env.item, &QRenderWindow::componentVisibilityChanged, &env.item,
        [&visibility](Index id, bool mesh_visible, bool geometry_visible) {
            visibility[id] = { mesh_visible, geometry_visible };
        });
    env.item.setGeometryVisibility(10, false);
    CHECK(visibility[10] == std::make_pair(false, false));
    env.loadGeometry(10);
    CHECK(visibility[10] == std::make_pair(false, false));
    env.item.reverseComponentVisibility(10);
    CHECK(visibility[10] == std::make_pair(false, true));
    REQUIRE(env.flush());
    SECTION("geometry removal clears presence")
    {
        env.model.findComponent(10)->geometry.reset();
        env.item.onComponentChanged(10);
    }
    SECTION("component removal clears presence")
    {
        env.model.removeComponent(10);
        env.item.deleteComponent(10);
    }
    SECTION("scene recreation preserves hidden geometry")
    {
        env.item.setGeometryVisibility(10, false);
        REQUIRE(env.flush());
        env.window.hide();
        env.window.releaseResources();
        QCoreApplication::processEvents();
        env.window.show();
        REQUIRE(env.flush());
        bool hidden = false;
        env.item.dispatch_async([&hidden](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
            auto* data = QRenderWindow::Data::SafeDownCast(user_data);
            auto actor = data->geometry_actor_manager_->getComponentActor(10);
            hidden = actor && !actor->isVisibilityEnabled();
        });
        REQUIRE(env.flush());
        CHECK(hidden);
        env.item.reverseComponentVisibility(10);
        CHECK(visibility[10] == std::make_pair(false, true));
        REQUIRE(env.flush());
        return;
    }
    // 已移除的目标不属于场景，反转应为空操作且不能重新创建显隐状态。
    visibility.clear();
    env.item.reverseComponentVisibility(10);
    CHECK(visibility.empty());
    REQUIRE(env.flush());
    bool geometry_removed = false;
    env.item.dispatch_async([&geometry_removed](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        geometry_removed = !data->geometry_actor_manager_->hasComponent(10);
    });
    REQUIRE(env.flush());
    CHECK(geometry_removed);
}

//! @brief 预加载显隐只保存 GUI 意图，首次初始化不能被更早的开关命令覆盖。
TEST_CASE("QRenderWindow preserves latest pre-load mesh visibility during scene initialization")
{
    qputenv("QSG_RENDER_LOOP", "threaded");
    testApplication();
    bool initial_visibility = false;
    bool final_visibility = true;
    bool update_after_load = true;
    SECTION("hide before loading then show") { }
    SECTION("show before loading then hide")
    {
        initial_visibility = true;
        final_visibility = false;
    }
    SECTION("retain hidden state from before loading")
    {
        final_visibility = false;
        update_after_load = false;
    }
    bool gui_visible = false;
    FaceHidingViewport env(false);
    QObject::connect(&env.item, &QRenderWindow::componentVisibilityChanged, &env.item,
        [&gui_visible](Index id, bool mesh_visible, bool) {
            if (id == 8)
                gui_visible = mesh_visible;
        });
    // 所有命令都先于首次场景初始化，初始化会先重放 GUI 缓存中的最新快照。
    env.item.setMeshVisibility(8, initial_visibility);
    env.item.loadMeshData(8, MakeMeshDataVtk(env.mesh));
    if (update_after_load)
        env.item.setMeshVisibility(8, final_visibility);
    CHECK(gui_visible == final_visibility);
    env.window.show();
    REQUIRE(env.flush());
    bool has_actor = false;
    bool actor_visible = false;
    env.item.dispatch_async([&has_actor, &actor_visible](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        auto actor = data->mesh_actor_manager_->getComponentActor(8);
        has_actor = actor != nullptr;
        actor_visible = actor && actor->isVisibilityEnabled();
    });
    REQUIRE(env.flush());
    REQUIRE(has_actor);
    CHECK(actor_visible == final_visibility);
    CHECK(gui_visible == final_visibility);
}

//! @brief 场景重建必须恢复窗口选择的样式，不能把隐藏、线框或透明重置为默认表面。
TEST_CASE("QRenderWindow preserves mesh and geometry styles across scene graph recreation")
{
    qputenv("QSG_RENDER_LOOP", "threaded");
    testApplication();
    MeshRenderStyle mesh_style = MeshRenderStyle::Hidden;
    GeometryRenderStyle geometry_style = GeometryRenderStyle::Hidden;
    SECTION("hidden styles") { }
    SECTION("wireframe styles")
    {
        mesh_style = MeshRenderStyle::WireframeSurface;
        geometry_style = GeometryRenderStyle::Wireframe;
    }
    SECTION("transparent styles")
    {
        mesh_style = MeshRenderStyle::Transparent50;
        geometry_style = GeometryRenderStyle::Transparent50;
    }
    // 标志先于视口构造，保证析构阶段的场景图信号也能安全访问。
    std::atomic_bool invalidated { false };
    std::atomic_bool recreated { false };
    FaceHidingViewport env;
    env.loadGeometry(10);
    env.item.setMeshStyle(static_cast<int>(mesh_style));
    env.item.setGeometryStyle(static_cast<int>(geometry_style));
    REQUIRE(env.flush());
    // 经渲染屏障读取实际 Actor 样式和可见性，不能只检查 GUI 属性值。
    auto check_styles = [&] {
        std::pair<int, int> styles { -1, -1 };
        std::pair<bool, bool> visible { false, false };
        env.item.dispatch_async([&styles, &visible](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
            auto* data = QRenderWindow::Data::SafeDownCast(user_data);
            if (auto mesh = data->mesh_actor_manager_->getComponentActor(7)) {
                styles.first = static_cast<int>(mesh->getRenderStyle());
                visible.first = mesh->isVisible();
            }
            if (auto geometry = data->geometry_actor_manager_->getComponentActor(10)) {
                styles.second = static_cast<int>(geometry->getRenderStyle());
                visible.second = geometry->isVisible();
            }
        });
        REQUIRE(env.flush());
        CAPTURE(styles.first, styles.second, static_cast<int>(mesh_style), static_cast<int>(geometry_style));
        CHECK(styles == std::make_pair(static_cast<int>(mesh_style), static_cast<int>(geometry_style)));
        CHECK(visible == std::make_pair(mesh_style != MeshRenderStyle::Hidden, geometry_style != GeometryRenderStyle::Hidden));
        CHECK(env.item.getMeshStyle() == static_cast<int>(mesh_style));
        CHECK(env.item.getGeometryStyle() == static_cast<int>(geometry_style));
    };
    check_styles();
    QObject::connect(&env.window, &QQuickWindow::sceneGraphInvalidated, &env.window, [&invalidated] { invalidated.store(true); }, Qt::DirectConnection);
    QObject::connect(&env.window, &QQuickWindow::sceneGraphInitialized, &env.window, [&recreated] { recreated.store(true); }, Qt::DirectConnection);
    env.window.hide();
    env.window.releaseResources();
    QElapsedTimer timer;
    timer.start();
    while (!invalidated.load() && timer.elapsed() < 5000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    REQUIRE(invalidated.load());
    env.window.show();
    REQUIRE(env.flush());
    REQUIRE(recreated.load());
    check_styles();
}

//! @brief 组件恢复只清除自身面约束，保留整体开关；组件反转不改变其他组件的隔离与几何状态。
TEST_CASE("QRenderWindow component face restore and reversal leave other components unchanged")
{
    testApplication();
    FaceHidingViewport env;
    env.loadGeometry(7);
    env.loadGeometry(10);
    auto second_mesh = env.mesh.clone();
    for (auto& point : second_mesh->vertex_positions_)
        point[1] += 1.5;
    env.item.loadMeshData(8, MakeMeshDataVtk(*second_mesh));
    REQUIRE(env.flush());
    env.selectFace(0);
    env.item.isolateSelectedFaces();
    REQUIRE(env.flush());
    env.item.setMeshVisibility(8, true);
    env.selectFace(1, 1.5);
    env.item.isolateSelectedFaces();
    env.item.setGeometryVisibility(10, true);
    REQUIRE(env.flush());
    REQUIRE(env.visibleFaceIds(7) == std::vector<Index>({ 0 }));
    REQUIRE(env.visibleFaceIds(8) == std::vector<Index>({ 1 }));

    int invalid_notifications = 0;
    QObject::connect(&env.item, &QRenderWindow::componentVisibilityChanged, &env.item,
        [&invalid_notifications](Index, bool, bool) { ++invalid_notifications; });
    env.item.reverseComponentVisibility(-1);
    env.item.reverseComponentVisibility(99);
    env.item.showAllHiddenFaces(99);
    CHECK(invalid_notifications == 0);
    env.item.showAllHiddenFaces(7);
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds(7) == std::vector<Index>({ 0, 1 }));
    CHECK(env.visibleFaceIds(8) == std::vector<Index>({ 1 }));
    bool gate_preserved = false;
    env.item.dispatch_async([&gate_preserved](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        gate_preserved = !data->mesh_actor_manager_->getComponentActor(7)->isVisibilityEnabled();
    });
    REQUIRE(env.flush());
    CHECK(gate_preserved);
    env.item.setMeshVisibility(7, true);
    REQUIRE(env.flush());
    bool restored_domain = false;
    env.item.dispatch_async([&restored_domain](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        MeshActorSelectOp first(data->mesh_actor_manager_->getComponentActor(7));
        MeshActorSelectOp second(data->mesh_actor_manager_->getComponentActor(8));
        restored_domain = first.getEdgeActor().GetVisibility() && !second.getEdgeActor().GetVisibility()
            && data->geometry_actor_manager_->getComponentActor(10)->isVisibilityEnabled();
    });
    REQUIRE(env.flush());
    CHECK(restored_domain);
    env.item.reverseComponentVisibility(7);
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds(8) == std::vector<Index>({ 1 }));
    bool unrelated_preserved = false;
    env.item.dispatch_async([&unrelated_preserved](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        unrelated_preserved = !data->mesh_actor_manager_->getComponentActor(7)->isVisibilityEnabled()
            && data->mesh_actor_manager_->getComponentActor(8)->isVisibilityEnabled()
            && data->geometry_actor_manager_->getComponentActor(10)->isVisibilityEnabled();
    });
    REQUIRE(env.flush());
    CHECK(unrelated_preserved);
}
