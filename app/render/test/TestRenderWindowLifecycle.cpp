/**
 * @file TestRenderWindowLifecycle.cpp
 * @brief 视口场景图销毁与重建的资源生命周期回归测试
 */
#include "QRenderWindow.h"
#include "QRenderWindowStyle.h"

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

        item.destroyingVTK(window, user_data);
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
        if (cycle == 0)
            item.setSelectionRevision(item.selectionRevision() + 1);
        QCoreApplication::processEvents();
        CHECK(updates == before + (cycle == 0 ? 0 : 1));
        CHECK(item.selectedIDs() == nullptr); // None 模式的空选择也正常发布

        item.destroyingVTK(window, user_data);
        window->RemoveRenderer(data->overlay_renderer_);
        window->RemoveRenderer(data->renderer_);
        user_data = nullptr;
    }
}
