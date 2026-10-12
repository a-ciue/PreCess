/**
 * @file TestRenderWindowFaceHiding.cpp
 * @brief 面隐藏与恢复时选择快照、高亮及面身份的视口回归测试
 */
#include "MakeMeshData.h"
#include "MakeMeshDataVtk.h"
#include "MeshActorManager.h"
#include "MeshActorSelectOp.h"
#include "QRenderWindow.h"
#include "QRenderWindowStyle.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QThread>
#include <catch2/catch_test_macros.hpp>
#include <vtkCamera.h>
#include <vtkCellData.h>
#include <vtkIdTypeArray.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindowInteractor.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <memory>
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
    void selectFace(Index face_id)
    {
        item.dispatch_async([face_id](vtkRenderWindow* render_window, QQuickVTKItem::vtkUserData user_data) {
            auto* data = QRenderWindow::Data::SafeDownCast(user_data);
            data->renderer_->SetWorldPoint(face_id == 0 ? 5.5 : 7.5, 0.5, 0.0, 1.0);
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

    //! @brief 在渲染调度中读取面过滤管线，避免 GUI 直接访问 VTK 状态。
    std::vector<Index> visibleFaceIds()
    {
        std::vector<Index> ids;
        item.dispatch_async([&ids](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
            auto* data = QRenderWindow::Data::SafeDownCast(user_data);
            MeshActorSelectOp select_op(data->mesh_actor_manager_->getComponentActor(7));
            auto* actor = vtkActor::SafeDownCast(&select_op.getFaceActor());
            auto* mapper = vtkPolyDataMapper::SafeDownCast(actor->GetMapper());
            mapper->Update();
            auto* face_ids = vtkIdTypeArray::SafeDownCast(
                mapper->GetInput()->GetCellData()->GetArray("PrecessFaceIds"));
            if (face_ids) {
                for (vtkIdType i = 0; i < face_ids->GetNumberOfTuples(); ++i)
                    ids.push_back(static_cast<Index>(face_ids->GetValue(i)));
            }
        });
        REQUIRE(flush());
        return ids;
    }
};

} // namespace

TEST_CASE("QRenderWindow hiding faces publishes an empty GUI selection")
{
    testApplication();
    FaceHidingViewport env;
    REQUIRE(env.flush());
    env.selectFace(0);
    REQUIRE(env.selectedFaceIds() == std::vector<Index>({ 0 }));

    int updates = 0;
    QObject::connect(&env.item, &QRenderWindow::selectedChanged, &env.item, [&updates] { ++updates; });
    env.item.hideSelectedFaces();
    REQUIRE(env.flush());

    CHECK(updates == 1);
    CHECK(env.selectedFaceIds().empty());
    CHECK(env.visibleFaceIds() == std::vector<Index>({ 1 }));
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

    // 原面 1 的渲染索引已变成 0；恢复后必须丢弃该索引，不能将它误解为原面 0。
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
