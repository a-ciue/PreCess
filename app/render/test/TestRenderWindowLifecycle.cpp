/**
 * @file TestRenderWindowLifecycle.cpp
 * @brief 视口场景图销毁与重建的资源生命周期回归测试
 */
#include "GeometryActor.h"
#include "GeometryActorManager.h"
#include "GeometryDataVtk.h"
#include "QRenderWindow.h"
#include "QRenderWindowStyle.h"

#include <BRepPrimAPI_MakeSphere.hxx>
#include <QGuiApplication>
#include <catch2/catch_test_macros.hpp>
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkWeakPointer.h>

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

TEST_CASE("QRenderWindow applies requested surface precision across scene rebuilds", "[precision]")
{
    testApplication();
    QRenderWindow item;
    int updates = 0;
    QObject::connect(&item, &QRenderWindow::surfacePrecisionChanged, &item, [&updates] { ++updates; });
    item.setSurfacePrecision(3);
    CHECK(item.getSurfacePrecision() == 3);
    CHECK(updates == 1);
    item.setSurfacePrecision(3); // 同档重复请求不再通知。
    item.setSurfacePrecision(-1);
    item.setSurfacePrecision(4);
    CHECK(item.getSurfacePrecision() == 3);
    CHECK(updates == 1);
    const auto sphere = BRepPrimAPI_MakeSphere(10.0).Shape();

    // 请求先于场景存在：初始化即把当前档位应用到新 manager 与后续加载的组件。
    vtkNew<vtkRenderWindow> window;
    vtkNew<vtkRenderWindowInteractor> interactor;
    interactor->SetRenderWindow(window);
    auto user_data = item.initializeVTK(window);
    auto* data = QRenderWindow::Data::SafeDownCast(user_data);
    data->geometry_actor_manager_->loadGeometry({ sphere, 123, nullptr });
    CHECK(data->geometry_actor_manager_->getSurfacePrecision() == 3);
    CHECK(data->geometry_actor_manager_->getComponentActor(123)->getSurfacePrecision() == 3);
    item.destroyingVTK(window, user_data);
    window->RemoveRenderer(data->overlay_renderer_);
    window->RemoveRenderer(data->renderer_);

    // 场景重建沿用当前请求档位，GUI 与渲染层保持一致。
    vtkNew<vtkRenderWindow> rebuilt_window;
    vtkNew<vtkRenderWindowInteractor> rebuilt_interactor;
    rebuilt_interactor->SetRenderWindow(rebuilt_window);
    user_data = item.initializeVTK(rebuilt_window);
    auto* rebuilt = QRenderWindow::Data::SafeDownCast(user_data);
    rebuilt->geometry_actor_manager_->loadGeometry({ sphere, 123, nullptr });
    QCoreApplication::processEvents();
    CHECK(item.getSurfacePrecision() == rebuilt->geometry_actor_manager_->getSurfacePrecision());
    CHECK(item.getSurfacePrecision() == 3);
    item.destroyingVTK(rebuilt_window, user_data);
    rebuilt_window->RemoveRenderer(rebuilt->overlay_renderer_);
    rebuilt_window->RemoveRenderer(rebuilt->renderer_);
}
