/**
 * @file TestRenderWindowLifecycle.cpp
 * @brief 视口场景图销毁与重建的资源生命周期回归测试
 */
#include "QRenderWindow.h"
#include "QRenderWindowStyle.h"
#include "ViewportOrientationRepresentation.h"
#include <vtkActor2D.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkImageData.h>
#include <vtkObjectFactory.h>
#include <vtkOpenGLTexture.h>
#include <vtkPropCollection.h>
#include <vtkTexturedActor2D.h>
#include <vtkTransform.h>

#include <QGuiApplication>
#include <catch2/catch_test_macros.hpp>
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkTextProperty.h>
#include <vtkWeakPointer.h>

#include <cmath>

namespace {
class CountingTexture : public vtkOpenGLTexture {
public:
    static CountingTexture* New();
    vtkTypeMacro(CountingTexture, vtkOpenGLTexture);
    int uploads {};
    void Render(vtkRenderer* renderer) override
    {
        const auto before = LoadTime.GetMTime();
        Superclass::Render(renderer);
        uploads += before != LoadTime.GetMTime();
    }
};
vtkStandardNewMacro(CountingTexture);

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

TEST_CASE("QRenderWindow keeps overlay logical sizes across DPR and VTK DPI changes")
{
    testApplication();
    class DprRenderWindow : public QRenderWindow {
    public:
        using QRenderWindow::itemChange;
    };
    DprRenderWindow item;
    for (const double ratio : { 1.0, 1.25, 1.5, 2.0, 1.0 }) {
        item.itemChange(QQuickItem::ItemDevicePixelRatioHasChanged, QQuickItem::ItemChangeData(ratio));
        for (const int dpi : { 72, 144 }) {
            INFO("DPR=" << ratio << ", VTK DPI=" << dpi);
            vtkNew<vtkRenderWindow> window;
            window->SetDPI(dpi);
            window->SetSize(1200, 900);
            vtkNew<vtkRenderWindowInteractor> interactor;
            interactor->SetRenderWindow(window);
            auto user_data = item.initializeVTK(window);
            auto* data = QRenderWindow::Data::SafeDownCast(user_data);
            auto* orientation = ViewportOrientationRepresentation::SafeDownCast(data->orientationWidget->GetRepresentation());
            REQUIRE(orientation != nullptr);
            const int expected_size = static_cast<int>(std::lround(120 * ratio));
            CHECK(orientation->GetSize()[0] == expected_size);
            CHECK(orientation->GetPadding()[0] == static_cast<int>(std::lround(12 * ratio)));
            CHECK(data->scale_bar_axis_->GetUseFontSizeFromProperty());
            const int font_size = data->scale_bar_axis_->GetLabelTextProperty()->GetFontSize();
            CHECK(font_size == static_cast<int>(std::lround(12 * ratio * 72 / dpi)));
            // widget 的视口和命中范围必须随 DPR 一起变，不能只放大贴图。
            data->orientationWidget->SquareResize();
            orientation->BuildRepresentation();
            CHECK(orientation->GetRenderer()->GetSize()[0] == expected_size);
            const int* origin = orientation->GetRenderer()->GetOrigin();
            CHECK(orientation->containsPoint(origin[0] + expected_size / 2, origin[1] + expected_size / 2));
            CHECK_FALSE(orientation->containsPoint(origin[0], origin[1]));
            window->SetSize(1600, 1000);
            data->orientationWidget->SquareResize();
            CHECK(orientation->GetRenderer()->GetSize()[0] == expected_size);
            CHECK(data->scale_bar_axis_->GetLabelTextProperty()->GetFontSize() == font_size);
            item.destroyingVTK(window, user_data);
            window->RemoveRenderer(data->overlay_renderer_);
            window->RemoveRenderer(data->renderer_);
            user_data = nullptr;
        }
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
    // 场景尚未创建时清空只排队，GUI 不提前发布完成通知。
    item.setSelectionRevision(1);
    item.setSelectionRevision(1);
    QCoreApplication::processEvents();
    CHECK(updates == 0);
    CHECK(item.selectedIDs() == nullptr);

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
            CHECK(updates == before); // GUI 不模拟清理完成，旧上下文的回传也不能更新新参数。
        }
        QCoreApplication::processEvents();
        CHECK(updates == before + (cycle == 0 ? 0 : 1));
        CHECK(item.selectedIDs() == nullptr); // None 模式的空选择也正常发布

        item.destroyingVTK(window, user_data);
        window->RemoveRenderer(data->overlay_renderer_);
        window->RemoveRenderer(data->renderer_);
        user_data = nullptr;
    }
}

TEST_CASE("Viewport orientation picks visible round handles in viewport coordinates")
{
    testApplication();
    vtkNew<vtkRenderWindow> window;
    window->SetSize(500, 300);
    vtkNew<vtkRenderer> renderer;
    renderer->SetViewport(0.6, 0.2, 0.84, 0.6);
    window->AddRenderer(renderer);
    vtkNew<ViewportOrientationRepresentation> orientation;
    orientation->SetRenderer(renderer);
    orientation->BuildRepresentation();
    const int* origin = renderer->GetOrigin();
    const int center_x = origin[0] + 60;
    const int center_y = origin[1] + 60;

    // 正负 Z 重叠时选择朝向观察者的一端，防止点到被遮住的轴。
    orientation->ComputeInteractionState(center_x, center_y, 1);
    CHECK(orientation->GetPickedAxis() == 2);
    CHECK(orientation->GetPickedDir() == 0);
    orientation->ComputeInteractionState(center_x + 38, center_y, 1);
    CHECK(orientation->GetPickedAxis() == 0);
    CHECK(orientation->GetPickedDir() == 0);
    orientation->ComputeInteractionState(center_x - 38, center_y, 1);
    CHECK(orientation->GetPickedAxis() == 0);
    CHECK(orientation->GetPickedDir() == 1);

    // 使用 VTK 原有的视角选择契约；连续点同轴可以切到反向。
    double event_position[2] { static_cast<double>(center_x - 38), static_cast<double>(center_y) };
    orientation->EndWidgetInteraction(event_position);
    CHECK(orientation->GetBack()[0] == 1.0);
    orientation->EndWidgetInteraction(event_position);
    CHECK(orientation->GetBack()[0] == -1.0);

    orientation->GetTransform()->RotateZ(90);
    orientation->BuildRepresentation();
    orientation->ComputeInteractionState(center_x, center_y + 38, 1);
    CHECK(orientation->GetPickedAxis() == 0);
    CHECK(orientation->GetPickedDir() == 0);
    orientation->ComputeInteractionState(origin[0], origin[1], 1);
    CHECK(orientation->GetInteractionStateAsEnum() == vtkCameraOrientationRepresentation::InteractionStateType::Outside);
    CHECK(orientation->GetPickedAxis() == -1);

    // 拖动仍由原控件处理，且离开圆点后继续完成本次旋转。
    orientation->StartWidgetInteraction(event_position);
    event_position[0] += 12;
    orientation->WidgetInteraction(event_position);
    CHECK(orientation->GetInteractionStateAsEnum() == vtkCameraOrientationRepresentation::InteractionStateType::Rotating);
    CHECK(orientation->GetAzimuth() == -9.0);
    orientation->ComputeInteractionState(0, 0, 0);
    CHECK(orientation->GetInteractionStateAsEnum() == vtkCameraOrientationRepresentation::InteractionStateType::Rotating);

    // 调整窗口后仍按实际像素拾取，不沿用旧视口原点或尺寸。
    orientation->EndWidgetInteraction(event_position);
    orientation->ComputeInteractionState(center_x, center_y + 38, 1);
    CHECK(orientation->GetInteractionStateAsEnum() == vtkCameraOrientationRepresentation::InteractionStateType::Hovering);
    orientation->ApplyInteractionState(vtkCameraOrientationRepresentation::InteractionStateType::Outside);
    window->SetSize(1000, 600);
    orientation->GetTransform()->Identity();
    orientation->BuildRepresentation();
    origin = renderer->GetOrigin();
    orientation->ComputeInteractionState(origin[0] + 196, origin[1] + 120, 1);
    CHECK(orientation->GetPickedAxis() == 0);
    CHECK(orientation->GetPickedDir() == 0);
}

TEST_CASE("Viewport orientation switches camera before and after dragging at a fixed size")
{
    testApplication();
    QRenderWindow item;
    vtkNew<vtkRenderWindow> window;
    window->SetSize(600, 400);
    vtkNew<vtkRenderWindowInteractor> interactor;
    interactor->SetRenderWindow(window);
    interactor->EnableRenderOff();
    auto user_data = item.initializeVTK(window);
    auto* data = QRenderWindow::Data::SafeDownCast(user_data);
    REQUIRE(data != nullptr);
    data->orientationWidget->SquareResize();
    auto* orientation = ViewportOrientationRepresentation::SafeDownCast(data->orientationWidget->GetRepresentation());
    REQUIRE(orientation != nullptr);
    int selections = 0;
    vtkNew<vtkCallbackCommand> selection_observer;
    selection_observer->SetClientData(&selections);
    selection_observer->SetCallback([](vtkObject*, unsigned long, void* client_data, void*) { ++*static_cast<int*>(client_data); });
    data->style_->AddObserver(vtkCommand::SelectionChangedEvent, selection_observer);
    auto* camera = data->renderer_->GetActiveCamera();
    for (int cycle = 0; cycle < 2; ++cycle) {
        if (cycle == 1)
            window->SetSize(1200, 800);
        data->orientationWidget->SquareResize();
        CHECK(orientation->GetRenderer()->GetSize()[0] == 120);
        CHECK(orientation->GetRenderer()->GetSize()[1] == 120);
        const int* origin = orientation->GetRenderer()->GetOrigin();
        const int x = origin[0] + 98;
        const int y = origin[1] + 60;
        camera->SetPosition(0, 0, 1);
        camera->SetViewUp(0, 1, 0);
        orientation->GetTransform()->Identity();
        orientation->BuildRepresentation();
        interactor->SetEventPosition(x, y);
        interactor->InvokeEvent(vtkCommand::MouseMoveEvent);
        interactor->InvokeEvent(vtkCommand::LeftButtonPressEvent);
        interactor->InvokeEvent(vtkCommand::LeftButtonReleaseEvent);
        const double* direction = camera->GetDirectionOfProjection();
        CHECK(direction[0] == -1.0);
        CHECK(direction[1] == 0.0);
        CHECK(direction[2] == 0.0);
        interactor->InvokeEvent(vtkCommand::MouseMoveEvent);
        interactor->InvokeEvent(vtkCommand::LeftButtonPressEvent);
        interactor->SetEventPosition(x + 12, y + 12);
        interactor->InvokeEvent(vtkCommand::MouseMoveEvent);
        CHECK(orientation->GetInteractionStateAsEnum() == vtkCameraOrientationRepresentation::InteractionStateType::Rotating);
        interactor->InvokeEvent(vtkCommand::LeftButtonReleaseEvent);
        CHECK(orientation->GetInteractionStateAsEnum() != vtkCameraOrientationRepresentation::InteractionStateType::Rotating);
    }
    CHECK(selections == 0);
    item.destroyingVTK(window, user_data);
}

TEST_CASE("Viewport orientation reuses its image for unchanged rotation and hover")
{
    testApplication();
    vtkNew<vtkRenderWindow> window;
    window->SetSize(120, 120);
    vtkNew<vtkRenderer> renderer;
    window->AddRenderer(renderer);
    vtkNew<ViewportOrientationRepresentation> orientation;
    orientation->SetRenderer(renderer);
    orientation->BuildRepresentation();
    vtkNew<vtkPropCollection> props;
    orientation->GetActors2D(props);
    REQUIRE(props->GetNumberOfItems() == 1);
    auto* actor = vtkTexturedActor2D::SafeDownCast(props->GetItemAsObject(0));
    REQUIRE(actor != nullptr);
    REQUIRE(actor->GetTexture() != nullptr);
    auto* image = actor->GetTexture()->GetInput();
    REQUIRE(image != nullptr);
    auto image_time = image->GetMTime();
    void* pixels = image->GetScalarPointer();
    // widget 会反复写相同旋转；图像及存储必须保持不变，避免重复上传。
    for (int i = 0; i < 20; ++i) {
        orientation->GetTransform()->Identity();
        orientation->GetTransform()->RotateZ(0);
        orientation->BuildRepresentation();
    }
    CHECK(image->GetMTime() == image_time);
    CHECK(image->GetScalarPointer() == pixels);
    // 正轴纹理和负轴圆环首帧即可见，验证原生分辨率缓存确实生成。
    auto* positive = static_cast<unsigned char*>(image->GetScalarPointer(103, 60, 0));
    auto* ring = static_cast<unsigned char*>(image->GetScalarPointer(12, 60, 0));
    CHECK(positive[3] > 200);
    CHECK(positive[0] > positive[1]);
    CHECK(ring[3] > 100);
    const int old_state = orientation->GetInteractionState();
    CHECK(orientation->containsPoint(98, 60));
    CHECK_FALSE(orientation->containsPoint(0, 0));
    CHECK(orientation->GetInteractionState() == old_state);
    orientation->ComputeInteractionState(98, 60, 1);
    orientation->BuildRepresentation();
    image_time = image->GetMTime();
    orientation->ComputeInteractionState(99, 60, 1);
    orientation->BuildRepresentation();
    CHECK(image->GetMTime() == image_time);
    orientation->GetTransform()->RotateZ(15);
    orientation->BuildRepresentation();
    CHECK(image->GetMTime() > image_time);
    CHECK(image->GetScalarPointer() == pixels);
    window->SetSize(0, 0);
    CHECK_FALSE(orientation->containsPoint(98, 60));
    CHECK(orientation->ComputeInteractionState(98, 60, 1) == static_cast<int>(vtkCameraOrientationRepresentation::InteractionStateType::Outside));
}

TEST_CASE("Viewport orientation uploads its persistent texture only when pixels change")
{
    testApplication();
    vtkNew<vtkRenderWindow> window;
    window->SetOffScreenRendering(1);
    window->SetMultiSamples(0);
    window->SetSize(120, 120);
    vtkNew<vtkRenderer> renderer;
    window->AddRenderer(renderer);
    vtkNew<ViewportOrientationRepresentation> orientation;
    orientation->SetRenderer(renderer);
    renderer->AddViewProp(orientation);
    orientation->BuildRepresentation();
    vtkNew<vtkPropCollection> props;
    orientation->GetActors2D(props);
    auto* actor = vtkTexturedActor2D::SafeDownCast(props->GetItemAsObject(0));
    REQUIRE(actor != nullptr);
    vtkNew<CountingTexture> texture;
    texture->SetInputData(actor->GetTexture()->GetInput());
    texture->SetColorModeToDirectScalars();
    texture->RepeatOff();
    actor->SetTexture(texture);
    window->Render();
    REQUIRE(texture->uploads == 1);
    for (int i = 0; i < 10; ++i) {
        orientation->GetTransform()->Identity();
        orientation->GetTransform()->RotateZ(0);
        window->Render();
    }
    CHECK(texture->uploads == 1);
    orientation->GetTransform()->RotateZ(15);
    window->Render();
    CHECK(texture->uploads == 2);
    orientation->ComputeInteractionState(60, 60, 1);
    window->Render();
    CHECK(texture->uploads == 3);
    orientation->ComputeInteractionState(61, 60, 1);
    window->Render();
    CHECK(texture->uploads == 3);
    window->SetSize(240, 240);
    window->Render();
    CHECK(texture->uploads == 4);
    CHECK(texture->GetInput()->GetDimensions()[0] == 240);
    CHECK(texture->GetInput()->GetDimensions()[1] == 240);
    orientation->ReleaseGraphicsResources(window);
    window->Render();
    CHECK(texture->uploads == 5);
}
