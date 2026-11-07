/**
 * @file TestRenderWindowLifecycle.cpp
 * @brief 视口场景图销毁与重建的资源生命周期回归测试
 */
#include "GeometryActor.h"
#include "GeometryActorManager.h"
#include "GeometryActorSelectOp.h"
#include "GeometryData.h"
#include "GeometryDataVtk.h"
#include "ModelLayer.h"
#include "ModelObserver.h"
#include "ModelScope.h"
#include "QModelQuery.h"
#include "QRenderWindow.h"
#include "QRenderWindowStyle.h"
#include "SessionQuery.h"
#include "UndoStack.h"
#include "ViewportOrientationRepresentation.h"
#include <vtkActor2D.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkCubeSource.h>
#include <vtkDisplaySizedImplicitPlaneRepresentation.h>
#include <vtkDisplaySizedImplicitPlaneWidget.h>
#include <vtkImageData.h>
#include <vtkObjectFactory.h>
#include <vtkOpenGLTexture.h>
#include <vtkPartitionedDataSet.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPropCollection.h>
#include <vtkTexturedActor2D.h>
#include <vtkTransform.h>

#include <BRepPrimAPI_MakeSphere.hxx>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QQuickWindow>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <gp_Pnt.hxx>
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkTextProperty.h>
#include <vtkWeakPointer.h>

#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

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
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
#ifdef Q_OS_WIN
        qputenv("QT_QPA_PLATFORM", "windows");
#else
        qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    }
    QQuickVTKItem::setGraphicsApi();
    static int argc = 1;
    static char app_name[] = "TestRenderWindowLifecycle";
    static char* argv[] = { app_name, nullptr };
    static QGuiApplication app(argc, argv);
    return app;
}

bool renderUntil(QQuickWindow& window, const std::function<bool()>& ready)
{
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < 10000) {
        window.requestUpdate();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return ready();
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

TEST_CASE("Clipping widget does not enlarge camera framing on reset or orientation clicks")
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

    vtkNew<vtkCubeSource> source;
    source->SetBounds(-0.05, 0.05, -0.05, 0.05, -0.05, 0.05);
    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputConnection(source->GetOutputPort());
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    data->renderer_->AddActor(actor);
    data->renderer_->ResetCamera();
    auto* camera = data->renderer_->GetActiveCamera();
    const double expected_scale = camera->GetParallelScale();
    double expected_bounds[6];
    data->renderer_->ComputeVisiblePropBounds(expected_bounds);

    // 使用实际视口初始化的控件；其屏幕尺寸自适应几何不应参与模型取景。
    auto* plane = data->plane_widget_->GetDisplaySizedImplicitPlaneRepresentation();
    plane->SetOrigin(0, 0, 0);
    data->plane_widget_->On();
    data->orientationWidget->SquareResize();
    auto* orientation = ViewportOrientationRepresentation::SafeDownCast(data->orientationWidget->GetRepresentation());
    REQUIRE(orientation != nullptr);

    SECTION("repeated reset restores the model scale after zooming")
    {
        camera->SetParallelScale(expected_scale * 3);
        for (int cycle = 0; cycle < 5; ++cycle) {
            INFO("reset cycle=" << cycle);
            plane->BuildRepresentation();
            data->renderer_->ResetCamera();
            CHECK(camera->GetParallelScale() == Catch::Approx(expected_scale));
        }
    }
    SECTION("orientation clicks reset framing without including the clipping widget")
    {
        camera->SetParallelScale(expected_scale * 3);
        for (int cycle = 0; cycle < 5; ++cycle) {
            INFO("orientation cycle=" << cycle);
            camera->SetPosition(0, 0, camera->GetDistance());
            camera->SetViewUp(0, 1, 0);
            orientation->GetTransform()->Identity();
            orientation->BuildRepresentation();
            const int* origin = orientation->GetRenderer()->GetOrigin();
            interactor->SetEventPosition(origin[0] + 98, origin[1] + 60);
            interactor->InvokeEvent(vtkCommand::MouseMoveEvent);
            interactor->InvokeEvent(vtkCommand::LeftButtonPressEvent);
            interactor->InvokeEvent(vtkCommand::LeftButtonReleaseEvent);
            CHECK(std::abs(camera->GetDirectionOfProjection()[0]) == 1.0);
            CHECK(camera->GetDirectionOfProjection()[1] == 0.0);
            CHECK(camera->GetDirectionOfProjection()[2] == 0.0);
            CHECK(camera->GetParallelScale() == Catch::Approx(expected_scale));
        }
    }

    double bounds[6];
    data->renderer_->ComputeVisiblePropBounds(bounds);
    for (int i = 0; i < 6; ++i)
        CHECK(bounds[i] == Catch::Approx(expected_bounds[i]));
    CHECK(data->plane_widget_->GetEnabled());
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

TEST_CASE("QRenderWindow applies requested surface precision across scene rebuilds", "[precision]")
{
    testApplication();
    QRenderWindow item;
    int updates = 0;
    QObject::connect(&item, &QRenderWindow::surfacePrecisionChanged, &item, [&updates] { ++updates; });
    item.setSurfacePrecision(3);
    CHECK(item.getSurfacePrecision() == 3);
    CHECK(updates == 1);
    item.setSurfacePrecision(3); // 同档设置无变化，不重复更新。
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
    auto actor = data->geometry_actor_manager_->getComponentActor(123);
    REQUIRE(actor != nullptr);
    auto* mapper = vtkPolyDataMapper::SafeDownCast(GeometryActorSelectOp(actor).getPolyActor().GetMapper());
    REQUIRE(mapper != nullptr);
    auto* polygons = mapper->GetInput();
    REQUIRE(polygons != nullptr);
    const auto triangle_count = polygons->GetNumberOfCells();
    CHECK(triangle_count > 0);
    actor.reset();
    item.destroyingVTK(window, user_data);
    window->RemoveRenderer(data->overlay_renderer_);
    window->RemoveRenderer(data->renderer_);

    // 场景重建沿用目标档位，并为真实几何恢复相同精度的显示数据。
    vtkNew<vtkRenderWindow> rebuilt_window;
    vtkNew<vtkRenderWindowInteractor> rebuilt_interactor;
    rebuilt_interactor->SetRenderWindow(rebuilt_window);
    user_data = item.initializeVTK(rebuilt_window);
    auto* rebuilt = QRenderWindow::Data::SafeDownCast(user_data);
    rebuilt->geometry_actor_manager_->loadGeometry({ sphere, 123, nullptr });
    QCoreApplication::processEvents();
    CHECK(item.getSurfacePrecision() == 3);
    auto rebuilt_actor = rebuilt->geometry_actor_manager_->getComponentActor(123);
    REQUIRE(rebuilt_actor != nullptr);
    auto* rebuilt_mapper = vtkPolyDataMapper::SafeDownCast(GeometryActorSelectOp(rebuilt_actor).getPolyActor().GetMapper());
    REQUIRE(rebuilt_mapper != nullptr);
    auto* rebuilt_polygons = rebuilt_mapper->GetInput();
    REQUIRE(rebuilt_polygons != nullptr);
    CHECK(rebuilt_polygons->GetNumberOfCells() == triangle_count);
    rebuilt_actor.reset();
    item.destroyingVTK(rebuilt_window, user_data);
    rebuilt_window->RemoveRenderer(rebuilt->overlay_renderer_);
    rebuilt_window->RemoveRenderer(rebuilt->renderer_);
}

TEST_CASE("QRenderWindow precision survives geometry undo redo and scene recreation", "[precision][render][undo]")
{
    testApplication();
    struct WindowObserver : ModelObserver {
        QRenderWindow* item {};
        void notifyModelChanged(Index) override { }
        void notifyModelAdded(Index) override { }
        void notifyModelRemoved(Index model_id) override
        {
            if (item)
                item->deleteModel(model_id);
        }
        void notifyComponentRemoved(Index component_id) override
        {
            if (item)
                item->deleteComponent(component_id);
        }
        void notifyComponentChanged(Index component_id) override
        {
            if (item)
                item->onComponentChanged(component_id);
        }
        void notifyModelNameChanged(Index, const std::string&) override { }
        void notifyGeometryLoadFailed(const std::string&) override { }
    } observer;
    ModelLayer layer(&observer);
    const auto model_id = layer.addModel("precision undo test", {});
    auto component = std::make_unique<ComponentData>();
    component->geometry = std::make_unique<GeometryData>();
    component->geometry->setRootShape(BRepPrimAPI_MakeSphere(10.0).Shape());
    const auto component_id = layer.getModelOperator(model_id)->addGeometryComponent(std::move(component));
    const auto original_face_id = layer.findComponent(component_id)->geometry->index.faceGlobalId(1);
    const auto original_solid_id = layer.findComponent(component_id)->geometry->index.solidGlobalId(1);
    UndoStack undo(layer);
    layer.setUndoRecorder(&undo);
    session::SessionQuery query(layer);
    QModelQuery model_query(&query);
    QQuickWindow quick_window;
    quick_window.setFlags(Qt::Tool | Qt::FramelessWindowHint);
    quick_window.resize(400, 400);
    quick_window.setPosition(0, 0);
    quick_window.setOpacity(0.01);
    quick_window.setPersistentSceneGraph(false);
    quick_window.setPersistentGraphics(false);
    // 由窗口在场景图退出后销毁子项，保证 destroyingVTK 仍可访问派生类服务。
    auto& item = *new QRenderWindow;
    item.setParent(quick_window.contentItem());
    item.setModelQuery(&model_query);
    observer.item = &item;
    item.onComponentChanged(component_id);
    item.setParentItem(quick_window.contentItem());
    item.setSize(QSizeF(400, 400));
    item.setSelectComponent(component_id);
    item.setSurfacePrecision(3);
    int target_updates = 0;
    int selection_updates = 0;
    QObject::connect(&item, &QRenderWindow::surfacePrecisionChanged, &item, [&] { ++target_updates; });
    QObject::connect(&item, &QRenderWindow::selectedChanged, &item, [&] { ++selection_updates; });
    struct RenderState {
        bool exists {};
        bool visible {};
        vtkIdType triangles {};
        vtkIdType highlight_cells {};
        std::array<double, 6> bounds {};
    } state;
    std::atomic_bool completed {};
    auto read_scene = [&] {
        completed = false;
        item.dispatch_async([&](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
            auto* data = QRenderWindow::Data::SafeDownCast(user_data);
            state = {};
            if (auto actor = data->geometry_actor_manager_->getComponentActor(component_id)) {
                auto& poly_actor = GeometryActorSelectOp(actor).getPolyActor();
                auto* polygons = vtkPolyDataMapper::SafeDownCast(poly_actor.GetMapper())->GetInput();
                state.exists = true;
                state.visible = actor->isVisible();
                state.triangles = polygons->GetNumberOfPolys();
                polygons->GetBounds(state.bounds.data());
            }
            auto* actors = data->renderer_->GetActors();
            actors->InitTraversal();
            while (auto* actor = actors->GetNextActor()) {
                if (actor->GetMapper()) {
                    if (auto* highlight = vtkPartitionedDataSet::SafeDownCast(actor->GetMapper()->GetInputDataObject(0, 0)))
                        state.highlight_cells += highlight->GetNumberOfCells();
                }
            }
            completed = true;
        });
        return renderUntil(quick_window, [&] { return completed.load(); });
    };
    auto selection_ids = [&] {
        const std::unique_ptr<QSelection> selected(item.selectedIDs());
        return selected && selected->get() ? selected->get()->ids : std::vector<Index> {};
    };
    auto pick_sphere = [&](const QString& mode, double center_x, double radius, Index expected_id) {
        item.clearSelection();
        item.setSelectMode(mode);
        completed = false;
        item.dispatch_async([&](vtkRenderWindow* window, QQuickVTKItem::vtkUserData user_data) {
            auto* data = QRenderWindow::Data::SafeDownCast(user_data);
            auto* camera = data->renderer_->GetActiveCamera();
            camera->SetPosition(center_x, 0.0, 100.0);
            camera->SetFocalPoint(center_x, 0.0, 0.0);
            camera->SetViewUp(0.0, 1.0, 0.0);
            camera->ParallelProjectionOn();
            data->renderer_->ResetCamera();
            window->Render();
            data->renderer_->SetWorldPoint(center_x + radius * 0.3, radius * 0.2,
                radius * std::sqrt(0.87), 1.0);
            data->renderer_->WorldToDisplay();
            const auto* position = data->renderer_->GetDisplayPoint();
            window->GetInteractor()->SetEventPosition(static_cast<int>(position[0]), static_cast<int>(position[1]));
            data->style_->SetClick();
            data->style_->OnLeftButtonUp();
            completed = true;
        });
        return renderUntil(quick_window, [&] {
            return completed.load() && selection_ids() == std::vector<Index> { expected_id };
        });
    };
    quick_window.show();
    REQUIRE(read_scene());
    REQUIRE(state.exists);
    CHECK(state.bounds[0] == Catch::Approx(-10.0).margin(0.1));
    CHECK(state.bounds[1] == Catch::Approx(10.0).margin(0.1));
    const auto fine_triangles = state.triangles;
    REQUIRE(fine_triangles > 0);
    REQUIRE(pick_sphere("GeometryFace", 0.0, 10.0, original_face_id));
    REQUIRE(read_scene());
    REQUIRE(state.highlight_cells > 0);

    const int updates_before_same_level = target_updates;
    item.setSurfacePrecision(3);
    REQUIRE(read_scene());
    CHECK(target_updates == updates_before_same_level);
    CHECK(selection_ids() == std::vector<Index> { original_face_id });

    // 正常清理入口发布空结果并清高亮；改变显示精度不生成模型历史。
    const int selections_before_switch = selection_updates;
    item.setSurfacePrecision(0);
    REQUIRE(renderUntil(quick_window, [&] {
        return selection_updates > selections_before_switch && selection_ids().empty();
    }));
    REQUIRE(read_scene());
    CHECK(state.highlight_cells == 0);
    REQUIRE(state.triangles < fine_triangles);
    const auto coarse_triangles = state.triangles;
    const auto coarse_bounds = state.bounds;
    CHECK_FALSE(undo.canUndo());
    REQUIRE(pick_sphere("GeometrySolid", 0.0, 10.0, original_solid_id));
    item.setSurfacePrecision(3);
    REQUIRE(read_scene());
    CHECK(state.triangles == fine_triangles);
    REQUIRE(pick_sphere("GeometryFace", 0.0, 10.0, original_face_id));

    {
        ModelScope operation(layer, &undo, "替换几何");
        layer.getComponentOperator(component_id)->replaceGeometryRoot(BRepPrimAPI_MakeSphere(gp_Pnt(25.0, 0.0, 0.0), 6.0).Shape());
    }
    const auto replaced_face_id = layer.findComponent(component_id)->geometry->index.faceGlobalId(1);
    const auto replaced_solid_id = layer.findComponent(component_id)->geometry->index.solidGlobalId(1);
    REQUIRE(replaced_face_id != original_face_id);
    REQUIRE(read_scene());
    CHECK(state.bounds[0] == Catch::Approx(19.0).margin(0.1));
    CHECK(state.bounds[1] == Catch::Approx(31.0).margin(0.1));
    REQUIRE(pick_sphere("GeometryFace", 25.0, 6.0, replaced_face_id));

    // 对真实历史执行 undo/redo：几何、gid 恢复，显示使用恢复时的窗口档位。
    item.setSurfacePrecision(0);
    REQUIRE(read_scene());
    const auto replaced_coarse_triangles = state.triangles;
    const auto replaced_coarse_bounds = state.bounds;
    REQUIRE(undo.undoLabel() == "替换几何");
    REQUIRE(undo.undo());
    REQUIRE(read_scene());
    // 显示边界由当前离散网格决定，与恢复前同几何、同档位的实际输出比较。
    for (std::size_t axis = 0; axis < coarse_bounds.size(); ++axis)
        CHECK(state.bounds[axis] == Catch::Approx(coarse_bounds[axis]).margin(1.0e-6));
    CHECK(state.triangles == coarse_triangles);
    CHECK(layer.findComponent(component_id)->geometry->index.faceGlobalId(1) == original_face_id);
    CHECK(layer.findComponent(component_id)->geometry->index.solidGlobalId(1) == original_solid_id);
    REQUIRE(pick_sphere("GeometryFace", 0.0, 10.0, original_face_id));
    REQUIRE(undo.redo());
    REQUIRE(read_scene());
    for (std::size_t axis = 0; axis < replaced_coarse_bounds.size(); ++axis)
        CHECK(state.bounds[axis] == Catch::Approx(replaced_coarse_bounds[axis]).margin(1.0e-6));
    CHECK(state.triangles == replaced_coarse_triangles);
    CHECK(layer.findComponent(component_id)->geometry->index.faceGlobalId(1) == replaced_face_id);
    REQUIRE(pick_sphere("GeometrySolid", 25.0, 6.0, replaced_solid_id));
    item.setSurfacePrecision(3);
    REQUIRE(read_scene());
    REQUIRE(undo.undo());
    REQUIRE(read_scene());
    CHECK(state.triangles == fine_triangles);
    REQUIRE(pick_sphere("GeometryFace", 0.0, 10.0, original_face_id));

    {
        ModelScope operation(layer, &undo, "删除组件");
        layer.removeComponent(component_id);
    }
    REQUIRE(read_scene());
    CHECK_FALSE(state.exists);
    REQUIRE(undo.undo());
    REQUIRE(read_scene());
    CHECK(state.exists);
    CHECK(state.triangles == fine_triangles);
    CHECK(layer.findComponent(component_id)->geometry->index.faceGlobalId(1) == original_face_id);
    REQUIRE(pick_sphere("GeometryFace", 0.0, 10.0, original_face_id));

    item.setComponentVisibility(component_id, false);
    REQUIRE(read_scene());
    CHECK_FALSE(state.visible);
    item.setComponentVisibility(component_id, true);
    REQUIRE(read_scene());
    CHECK(state.visible);
    REQUIRE(pick_sphere("GeometryFace", 0.0, 10.0, original_face_id));

    std::atomic_bool scene_invalidated {};
    const auto invalidated_connection = QObject::connect(&quick_window, &QQuickWindow::sceneGraphInvalidated, &quick_window, [&] { scene_invalidated = true; }, Qt::DirectConnection);
    quick_window.hide();
    quick_window.releaseResources();
    REQUIRE(renderUntil(quick_window, [&] { return scene_invalidated.load(); }));
    QObject::disconnect(invalidated_connection);
    quick_window.show();
    REQUIRE(read_scene());
    CHECK(state.exists);
    CHECK(state.triangles == fine_triangles);
    CHECK(item.getSurfacePrecision() == 3);
    REQUIRE(pick_sphere("GeometryFace", 0.0, 10.0, original_face_id));
    observer.item = nullptr;
    layer.setUndoRecorder(nullptr);
}
