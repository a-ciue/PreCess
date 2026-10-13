/**
 * @file TestRenderWindowFaceHiding.cpp
 * @brief 面隐藏与恢复时选择快照、高亮及面身份的视口回归测试
 */
#include "MakeMeshData.h"
#include "MakeMeshDataVtk.h"
#include "MeshActorManager.h"
#include "MeshActorSelectOp.h"
#include "GeometryActor.h"
#include "GeometryActorManager.h"
#include "QRenderWindow.h"
#include "QRenderWindowStyle.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QThread>
#include <BRep_Builder.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Vertex.hxx>
#include <gp_Pnt.hxx>
#include <catch2/catch_test_macros.hpp>
#include <vtkCamera.h>
#include <vtkActorCollection.h>
#include <vtkCellData.h>
#include <vtkCompositePolyDataMapper.h>
#include <vtkIdTypeArray.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindowInteractor.h>

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
    QQuickWindow window;
    QRenderWindow item;

    //! @brief 加载原面 0、1；隐藏面 0 后，面 1 的渲染单元索引会压缩为 0。
    FaceHidingViewport()
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
        item.dispatch_async([this](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
            auto* data = QRenderWindow::Data::SafeDownCast(user_data);
            auto model_data = MakeMeshDataVtk(mesh);
            data->mesh_actor_manager_->loadMesh(7, model_data, data->renderer_);
            auto* camera = data->renderer_->GetActiveCamera();
            camera->SetPosition(6.5, 0.5, 5.0);
            camera->SetFocalPoint(6.5, 0.5, 0.0);
            camera->SetViewUp(0.0, 1.0, 0.0);
            camera->SetParallelScale(2.0);
        });
        item.setSelectMode("Face");
        window.show();
    }

    //! @brief 在视口对象析构前释放场景图，确保 VTK 服务按原有生命周期清理。
    ~FaceHidingViewport()
    {
        window.hide();
        window.releaseResources();
        QCoreApplication::processEvents();
    }

    //! @brief 等待已入队的渲染命令完成，再消费它们发布的 GUI 选择快照。
    bool flush()
    {
        auto completed = std::make_shared<std::atomic_bool>(false);
        item.dispatch_async([completed](vtkRenderWindow*, QQuickVTKItem::vtkUserData) {
            completed->store(true);
        });
        QElapsedTimer timer;
        timer.start();
        while (!completed->load() && timer.elapsed() < 5000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            QThread::msleep(1);
        }
        QCoreApplication::processEvents();
        return completed->load();
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
    env.item.dispatch_async([&env, &second_mesh](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        data->mesh_actor_manager_->loadMesh(8, MakeMeshDataVtk(*second_mesh), data->renderer_);
    });
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

TEST_CASE("QRenderWindow retained hidden face selection follows explicit clearing boundaries")
{
    testApplication();
    FaceHidingViewport env;
    REQUIRE(env.flush());
    env.selectFace(0);
    env.item.hideSelectedFaces();
    REQUIRE(env.flush());
    REQUIRE(env.selectedFaceIds() == std::vector<Index>({ 0 }));
    SECTION("clear selection") { env.item.clearSelection(); }
    SECTION("change selection mode") { env.item.setSelectMode("Component"); }
    SECTION("change selection revision") { env.item.setSelectionRevision(1); }
    REQUIRE(env.flush());
    CHECK(env.selectedFaceIds().empty());
    env.item.showSelectedFaces();
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 1 }));
}

TEST_CASE("QRenderWindow hiding faces preserves component selection")
{
    testApplication();
    FaceHidingViewport env;
    REQUIRE(env.flush());
    env.item.setSelectMode("Component");
    REQUIRE(env.flush());
    // 在面位置真实拾取整个组件，验证非面模式下隐藏接口不会清除组件选择。
    env.selectFace(0);
    std::unique_ptr<QSelection> before(env.item.selectedIDs());
    REQUIRE(before != nullptr);
    REQUIRE(before->get() != nullptr);
    REQUIRE(before->get()->type == ElementEnum::Component);
    REQUIRE(before->get()->ids == std::vector<Index>({ 7 }));

    int updates = 0;
    QObject::connect(&env.item, &QRenderWindow::selectedChanged, &env.item, [&updates] { ++updates; });
    env.item.hideSelectedFaces();
    REQUIRE(env.flush());

    std::unique_ptr<QSelection> after(env.item.selectedIDs());
    REQUIRE(after != nullptr);
    REQUIRE(after->get() != nullptr);
    CHECK(after->get()->type == before->get()->type);
    CHECK(after->get()->ids == before->get()->ids);
    CHECK(after->get()->component_id == before->get()->component_id);
    CHECK(updates == 0);
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 0, 1 }));
}

TEST_CASE("QRenderWindow hiding faces without selection does not publish a reset")
{
    testApplication();
    FaceHidingViewport env;
    REQUIRE(env.flush());
    REQUIRE(env.selectedFaceIds().empty());
    int updates = 0;
    QObject::connect(&env.item, &QRenderWindow::selectedChanged, &env.item, [&updates] { ++updates; });

    // 空选择不产生隐藏操作，也不应发布一次额外的 GUI 选择清理。
    env.item.hideSelectedFaces();
    REQUIRE(env.flush());
    CHECK(env.selectedFaceIds().empty());
    CHECK(updates == 0);
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
    env.item.showAllHiddenFaces();
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

TEST_CASE("QRenderWindow reversing visibility swaps hidden faces in every selection mode")
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
    SECTION("Vertex") { mode = "Vertex"; }
    SECTION("Edge") { mode = "Edge"; }
    SECTION("Solid") { mode = "Solid"; }
    env.item.setSelectMode(mode);
    env.selectFace(1);
    env.item.reverseVisibility({ 7 });
    REQUIRE(env.flush());
    CHECK(env.selectedFaceIds().empty());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 0 }));

    // 隐藏面不能拾取；重新显示的面仍返回原模型面 id。
    env.item.setSelectMode("Face");
    env.selectFace(1);
    CHECK(env.selectedFaceIds().empty());
    env.selectFace(0);
    CHECK(env.selectedFaceIds() == std::vector<Index>({ 0 }));
    env.item.reverseVisibility({ 7 });
    REQUIRE(env.flush());
    CHECK(env.selectedFaceIds().empty());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 1 }));

    // 不等待对象树回传即连续点击，仍应逐次基于渲染线程当前状态取反。
    env.item.reverseVisibility({ 7 });
    env.item.reverseVisibility({ 7 });
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 1 }));
    env.item.showAllHiddenFaces();
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
    env.item.dispatch_async([&env](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        auto model_data = MakeMeshDataVtk(env.mesh);
        data->mesh_actor_manager_->loadMesh(8, model_data, data->renderer_);
        data->mesh_actor_manager_->hideFaces(8, { 0 });
        data->mesh_actor_manager_->setVisibility(8, false);
        data->mesh_actor_manager_->loadMesh(9, model_data, data->renderer_);
    });
    REQUIRE(env.flush());
    std::map<Index, std::pair<bool, bool>> visibility;
    QObject::connect(&env.item, &QRenderWindow::componentVisibilityChanged, &env.item,
                     [&visibility](Index component_id, bool mesh_visible, bool geometry_visible) {
                         visibility[component_id] = { mesh_visible, geometry_visible };
                     });
    env.item.reverseVisibility({ 7, 8, 9, 7 });
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
    env.item.dispatch_async([mesh_enabled, geometry_enabled, hidden_style](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        BRep_Builder builder;
        TopoDS_Compound shape;
        TopoDS_Vertex vertex;
        builder.MakeCompound(shape);
        builder.MakeVertex(vertex, gp_Pnt(100.0, 100.0, 0.0), 1.0e-7);
        builder.Add(shape, vertex);
        data->geometry_actor_manager_->loadGeometry({ shape, 7 });
        data->mesh_actor_manager_->setVisibility(7, mesh_enabled);
        data->geometry_actor_manager_->setVisibility(7, geometry_enabled);
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
    env.item.reverseVisibility({ 7 });
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
    env.item.reverseVisibility({ 7 });
    env.item.reverseVisibility({ 7 });
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
    env.selectFace(0);
    second_mesh = env.mesh.clone();
    for (auto& point : second_mesh->vertex_positions_)
        point[1] += 1.5;
    env.item.dispatch_async([&env, &second_mesh](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        data->mesh_actor_manager_->loadMesh(8, MakeMeshDataVtk(*second_mesh), data->renderer_);
        data->mesh_actor_manager_->hideFaces(8, { 1 });
        data->mesh_actor_manager_->loadMesh(9, MakeMeshDataVtk(env.mesh), data->renderer_);
        // 纯几何组件也属于全场景隔离范围，不能只遍历网格管理器。
        BRep_Builder builder;
        TopoDS_Compound shape;
        TopoDS_Vertex vertex;
        builder.MakeCompound(shape);
        builder.MakeVertex(vertex, gp_Pnt(100.0, 100.0, 0.0), 1.0e-7);
        builder.Add(shape, vertex);
        data->geometry_actor_manager_->loadGeometry({ shape, 7 });
        data->geometry_actor_manager_->loadGeometry({ shape, 10 });
    });
    REQUIRE(env.flush());
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
    env.item.reverseVisibility({ 7, 8, 9, 10 });
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds(7) == std::vector<Index>({ 1 }));
    CHECK(env.visibleFaceIds(8) == std::vector<Index>({ 1 }));
    CHECK(visibility[7] == std::make_pair(true, false));
    CHECK(visibility[9] == std::make_pair(true, false));
    CHECK(visibility[10] == std::make_pair(false, true));
    env.item.showAllHiddenFaces();
    env.item.setMeshVisibility(9, true);
    env.item.setGeometryVisibility(10, true);
    REQUIRE(env.flush());
    CHECK(env.visibleFaceIds(7) == std::vector<Index>({ 0, 1 }));
    CHECK(env.visibleFaceIds(8) == std::vector<Index>({ 0, 1 }));
}

TEST_CASE("QRenderWindow face display and isolation preserve non-face or empty selections")
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
    const auto before = env.selectedFaceIds();
    int updates = 0;
    QObject::connect(&env.item, &QRenderWindow::selectedChanged, &env.item, [&updates] { ++updates; });
    env.item.showSelectedFaces();
    env.item.isolateSelectedFaces();
    REQUIRE(env.flush());
    CHECK(env.selectedFaceIds() == before);
    CHECK(updates == 0);
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 0, 1 }));
}
