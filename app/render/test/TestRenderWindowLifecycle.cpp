/**
 * @file TestRenderWindowLifecycle.cpp
 * @brief 视口场景图销毁与重建的资源生命周期回归测试
 */
#include "GeometryActor.h"
#include "GeometryActorManager.h"
#include "GeometryDataVtk.h"
#include "QRenderWindow.h"
#include "QRenderWindowStyle.h"
#include "SurfacePrecisionFailure.h"

#include <BRepPrimAPI_MakeSphere.hxx>
#include <QGuiApplication>
#include <catch2/catch_test_macros.hpp>
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkWeakPointer.h>

struct QRenderWindowTestAccess {
    static quint64 revision(const QRenderWindow& item) { return item.surface_precision_revision_; }
    static void apply(QRenderWindow& item, QQuickVTKItem::vtkUserData data, int precision, quint64 revision)
    {
        item.applySurfacePrecision(data, precision, revision);
    }
};

namespace {
QGuiApplication& testApplication()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QQuickVTKItem::setGraphicsApi();
    static int argc = 1;
    static char app_name[] = "TestRenderWindowLifecycle";
    static char* argv[] = { app_name, nullptr };
    static QGuiApplication app(argc, argv);
    return app;
}
}

TEST_CASE("QRenderWindow releases interaction actors before recreating VTK resources")
{
    testApplication();
    QRenderWindow item;

    // 重放 QQuickVTKItem 的初始化/销毁边界，不启动实际渲染。
    for (int cycle = 0; cycle < 3; ++cycle) {
        vtkNew<vtkRenderWindow> window;
        vtkNew<vtkRenderWindowInteractor> interactor;
        interactor->SetRenderWindow(window);
        auto user_data = item.initializeVTK(window);
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        REQUIRE(data != nullptr);
        auto* actors = data->renderer_->GetActors();
        // InteractionService 添加点、实线、虚线三个标注 actor；renderer 还可包含选择高亮等 actor。
        REQUIRE(actors->GetNumberOfItems() >= 3);
        vtkWeakPointer<vtkActor> interaction_actor = actors->GetLastActor();
        REQUIRE(interaction_actor.GetPointer() != nullptr);

        REQUIRE(data->style_->HasObserver(vtkCommand::SelectionChangedEvent));
        item.destroyingVTK(window, user_data);
        CHECK_FALSE(data->style_->HasObserver(vtkCommand::SelectionChangedEvent));
        // 即使外部仍持有 style，销毁后的事件也不得访问已释放的服务。
        data->style_->InvokeEvent(vtkCommand::SelectionChangedEvent);
        // RemoveAllViewProps 只能释放 renderer 持有的引用，服务本身也必须及时析构。
        CHECK(interaction_actor.GetPointer() == nullptr);
        window->RemoveRenderer(data->overlay_renderer_);
        window->RemoveRenderer(data->renderer_);
        user_data = nullptr;
    }
}

TEST_CASE("QRenderWindow queues selection updates and discards results for old parameters")
{
    testApplication();
    QRenderWindow item;
    int updates = 0;
    QObject::connect(&item, &QRenderWindow::selectedChanged, &item, [&updates] { ++updates; });

    for (int cycle = 0; cycle < 2; ++cycle) {
        vtkNew<vtkRenderWindow> window;
        vtkNew<vtkRenderWindowInteractor> interactor;
        interactor->SetRenderWindow(window);
        auto user_data = item.initializeVTK(window);
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        REQUIRE(data != nullptr);
        QCoreApplication::processEvents();
        const int before = updates;

        // 选择完成通知只能在 GUI 事件队列消费后送达；切换参数则丢弃旧版本快照。
        data->style_->SetClick();
        data->style_->OnLeftButtonUp();
        CHECK(updates == before);
        if (cycle == 0) {
            item.setSelectionRevision(item.selectionRevision() + 1);
            CHECK(updates == before + 1); // 重置通知同步送达
        }
        QCoreApplication::processEvents();
        CHECK(updates == before + 1);
        CHECK(item.selectedIDs() == nullptr); // None 模式的空选择也正常发布

        item.destroyingVTK(window, user_data);
        window->RemoveRenderer(data->overlay_renderer_);
        window->RemoveRenderer(data->renderer_);
        user_data = nullptr;
    }
}

TEST_CASE("QRenderWindow notifies selection reset without a render window")
{
    testApplication();
    QRenderWindow item;
    int updates = 0;
    QObject::connect(&item, &QRenderWindow::selectedChanged, &item, [&updates] { ++updates; });
    item.setSelectionRevision(1);
    CHECK(updates == 1);
    CHECK(item.selectedIDs() == nullptr);
    item.setSelectionRevision(1);
    QCoreApplication::processEvents();
    CHECK(updates == 1);
}

TEST_CASE("QRenderWindow contains precision exceptions restores GUI state and retries", "[precision][failure]")
{
    testApplication();
    auto kind = SurfacePrecisionFailure::Kind::Standard;
    SECTION("standard exception") { }
    SECTION("OCCT exception") { kind = SurfacePrecisionFailure::Kind::Occ; }
    SECTION("unknown exception") { kind = SurfacePrecisionFailure::Kind::Unknown; }
    QRenderWindow item;
    vtkNew<vtkRenderWindow> window;
    vtkNew<vtkRenderWindowInteractor> interactor;
    interactor->SetRenderWindow(window);
    auto user_data = item.initializeVTK(window);
    auto* data = QRenderWindow::Data::SafeDownCast(user_data);
    const auto sphere = BRepPrimAPI_MakeSphere(10.0).Shape();
    data->geometry_actor_manager_->loadGeometry({ sphere, 123, nullptr });
    QCoreApplication::processEvents();
    int updates = 0;
    QObject::connect(&item, &QRenderWindow::surfacePrecisionChanged, &item, [&updates] { ++updates; });
    item.setSurfacePrecision(3);
    CHECK(item.getSurfacePrecision() == 3);
    CHECK(updates == 1);
    SurfacePrecisionFailure failure(1, kind);
    REQUIRE_NOTHROW(QRenderWindowTestAccess::apply(item, user_data, 3, QRenderWindowTestAccess::revision(item)));
    CHECK(data->geometry_actor_manager_->getSurfacePrecision() == 1);
    CHECK(item.getSurfacePrecision() == 3); // 渲染线程不能直接改 GUI 属性。
    QCoreApplication::processEvents();
    CHECK(item.getSurfacePrecision() == 1);
    CHECK(updates == 2);
    const auto failed_revision = QRenderWindowTestAccess::revision(item);
    item.setSurfacePrecision(3);
    CHECK(QRenderWindowTestAccess::revision(item) > failed_revision);
    REQUIRE_NOTHROW(QRenderWindowTestAccess::apply(item, user_data, 3, QRenderWindowTestAccess::revision(item)));
    QCoreApplication::processEvents();
    CHECK(item.getSurfacePrecision() == 3);
    CHECK(data->geometry_actor_manager_->getSurfacePrecision() == 3);
    CHECK(data->geometry_actor_manager_->getComponentActor(123)->getSurfacePrecision() == 3);
    CHECK(updates == 3);
    item.destroyingVTK(window, user_data);
    window->RemoveRenderer(data->overlay_renderer_);
    window->RemoveRenderer(data->renderer_);
}

TEST_CASE("QRenderWindow ignores stale precision failure after a newer request", "[precision][failure]")
{
    testApplication();
    QRenderWindow item;
    vtkNew<vtkRenderWindow> window;
    vtkNew<vtkRenderWindowInteractor> interactor;
    interactor->SetRenderWindow(window);
    auto user_data = item.initializeVTK(window);
    auto* data = QRenderWindow::Data::SafeDownCast(user_data);
    const auto sphere = BRepPrimAPI_MakeSphere(10.0).Shape();
    data->geometry_actor_manager_->loadGeometry({ sphere, 123, nullptr });
    QCoreApplication::processEvents();
    item.setSurfacePrecision(3);
    SurfacePrecisionFailure failure;
    QRenderWindowTestAccess::apply(item, user_data, 3, QRenderWindowTestAccess::revision(item));
    item.setSurfacePrecision(0);
    QCoreApplication::processEvents();
    CHECK(item.getSurfacePrecision() == 0);
    QRenderWindowTestAccess::apply(item, user_data, 0, QRenderWindowTestAccess::revision(item));
    QCoreApplication::processEvents();
    CHECK(item.getSurfacePrecision() == 0);
    CHECK(data->geometry_actor_manager_->getSurfacePrecision() == 0);
    item.destroyingVTK(window, user_data);
    window->RemoveRenderer(data->overlay_renderer_);
    window->RemoveRenderer(data->renderer_);
}

TEST_CASE("QRenderWindow ignores precision failure from a destroyed scene", "[precision][failure][lifecycle]")
{
    testApplication();
    QRenderWindow item;
    vtkNew<vtkRenderWindow> old_window;
    vtkNew<vtkRenderWindowInteractor> old_interactor;
    old_interactor->SetRenderWindow(old_window);
    auto old_user_data = item.initializeVTK(old_window);
    auto* old_data = QRenderWindow::Data::SafeDownCast(old_user_data);
    const auto sphere = BRepPrimAPI_MakeSphere(10.0).Shape();
    old_data->geometry_actor_manager_->loadGeometry({ sphere, 123, nullptr });
    QCoreApplication::processEvents();
    item.setSurfacePrecision(3);
    SurfacePrecisionFailure failure;
    QRenderWindowTestAccess::apply(item, old_user_data, 3, QRenderWindowTestAccess::revision(item));
    CHECK(old_data->geometry_actor_manager_->getSurfacePrecision() == 1);
    CHECK(item.getSurfacePrecision() == 3);
    // 旧失败回执尚未消费时重建场景，新 manager 已采用当前请求档位。
    item.destroyingVTK(old_window, old_user_data);
    old_window->RemoveRenderer(old_data->overlay_renderer_);
    old_window->RemoveRenderer(old_data->renderer_);

    vtkNew<vtkRenderWindow> window;
    vtkNew<vtkRenderWindowInteractor> interactor;
    interactor->SetRenderWindow(window);
    auto user_data = item.initializeVTK(window);
    auto* data = QRenderWindow::Data::SafeDownCast(user_data);
    data->geometry_actor_manager_->loadGeometry({ sphere, 123, nullptr });
    CHECK(data->geometry_actor_manager_->getSurfacePrecision() == 3);
    QCoreApplication::processEvents();
    CHECK(item.getSurfacePrecision() == data->geometry_actor_manager_->getSurfacePrecision());
    CHECK(item.getSurfacePrecision() == 3);
    item.destroyingVTK(window, user_data);
    window->RemoveRenderer(data->overlay_renderer_);
    window->RemoveRenderer(data->renderer_);
}

TEST_CASE("QRenderWindow corrects precision requests queued before scene initialization", "[precision][failure][lifecycle]")
{
    testApplication();
    QRenderWindow item;
    item.setSurfacePrecision(0);
    const auto first_revision = QRenderWindowTestAccess::revision(item);
    item.setSurfacePrecision(3);
    const auto latest_revision = QRenderWindowTestAccess::revision(item);
    vtkNew<vtkRenderWindow> window;
    vtkNew<vtkRenderWindowInteractor> interactor;
    interactor->SetRenderWindow(window);
    auto user_data = item.initializeVTK(window);
    auto* data = QRenderWindow::Data::SafeDownCast(user_data);
    const auto sphere = BRepPrimAPI_MakeSphere(10.0).Shape();
    data->geometry_actor_manager_->loadGeometry({ sphere, 123, nullptr });
    QCoreApplication::processEvents();
    CHECK(data->geometry_actor_manager_->getSurfacePrecision() == 3);
    // 初始化不作废请求版本；较早请求生效、最新请求失败后仍应校正 GUI。
    QRenderWindowTestAccess::apply(item, user_data, 0, first_revision);
    SurfacePrecisionFailure failure;
    QRenderWindowTestAccess::apply(item, user_data, 3, latest_revision);
    CHECK(data->geometry_actor_manager_->getSurfacePrecision() == 0);
    QCoreApplication::processEvents();
    CHECK(item.getSurfacePrecision() == 0);
    item.destroyingVTK(window, user_data);
    window->RemoveRenderer(data->overlay_renderer_);
    window->RemoveRenderer(data->renderer_);
}
