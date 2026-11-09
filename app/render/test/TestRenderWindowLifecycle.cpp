/**
 * @file TestRenderWindowLifecycle.cpp
 * @brief 视口场景图销毁与重建的资源生命周期回归测试
 */
#include "MakeMeshData.h"
#include "MakeMeshDataVtk.h"
#include "MeshActorManager.h"
#include "QRenderWindow.h"
#include "QRenderWindowStyle.h"
#include "ViewportOrientationRepresentation.h"
#include "renderStrategy/AttributeOperator.h"
#include <vtkActor2D.h>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkCellData.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkImageData.h>
#include <vtkIdTypeArray.h>
#include <vtkObjectFactory.h>
#include <vtkOpenGLTexture.h>
#include <vtkPolyData.h>
#include <vtkPropCollection.h>
#include <vtkTexturedActor2D.h>
#include <vtkTransform.h>

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlExpression>
#include <catch2/catch_test_macros.hpp>
#include <vtkActor.h>
#include <vtkActorCollection.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkScalarBarActor.h>
#include <vtkTextProperty.h>
#include <vtkWeakPointer.h>

#include <cmath>
#include <limits>
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
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QQuickVTKItem::setGraphicsApi();
    static int argc = 1;
    static char app_name[] = "TestRenderWindowLifecycle";
    static char* argv[] = { app_name, nullptr };
    static QGuiApplication app(argc, argv);
    return app;
}
}

TEST_CASE("Scalar range arguments reject invalid input without changing active state", "[scalar-range]")
{
    testApplication();
    // 仅调用 GUI 参数入口，不创建 QQuickWindow，也不初始化或执行 VTK 渲染。
    QRenderWindow item;
    auto mesh = MakeMeshDataWithAtri();
    item.loadMeshData(7, MakeMeshDataVtk(mesh));
    const int scalar = static_cast<int>(Mode::SCALAR);
    REQUIRE(item.setAttriMode(7, "f_scalar_1", scalar, { { "scalar_range", QVariantList { 0.25, 0.75 } }, { "display_range", QVariantList { 0.25, 0.75 } } }));
    const auto original = item.scalarRangeInfo(7, "f_scalar_1");
    REQUIRE(original.size() == 2);
    const QVariantList invalid_limits {
        QVariantList { 0.0 }, QVariantList { 0.0, 1.0, 2.0 },
        QVariantList { std::numeric_limits<double>::quiet_NaN(), 1.0 },
        QVariantList { 0.0, std::numeric_limits<double>::infinity() }, QVariantList { 2.0, 1.0 },
        QVariantList { "0", 1.0 }, QVariantList { false, 1.0 }, QVariantList { QVariant(), 1.0 },
        QString("0,1"), 1.0, QVariantMap { { "lower", 0.0 }, { "upper", 1.0 } }
    };
    // 字典入口先拒绝错误形状和非数值端点；数据侧继续校验有限性与顺序。
    for (const auto& limits : invalid_limits) {
        CHECK_FALSE(item.setAttriMode(7, "f_scalar_1", scalar, { { "scalar_range", limits } }));
        CHECK_FALSE(item.setAttriMode(7, "f_scalar_1", scalar, { { "display_range", limits } }));
    }
    CHECK_FALSE(item.setAttriMode(7, "missing", scalar, { }));
    CHECK_FALSE(item.setAttriMode(7, "f_normal_3", scalar, { { "display_range", QVariantList { 0.0, 1.0 } } }));
    CHECK(item.scalarRangeInfo(7, "f_scalar_1") == original);
    // 缺省映射、多分量、相等端点及超出原始值域继续支持。
    REQUIRE(item.setAttriMode(7, "f_normal_3", scalar, { }));
    REQUIRE(item.setAttriMode(7, "f_scalar_1", scalar, { { "scalar_range", QVariantList { -2.5, 3.5 } } }));
    REQUIRE(item.setAttriMode(7, "f_scalar_1", scalar, { { "scalar_range", QVariantList { 0.0, 0.0 } } }));
    // 真实 QML 字典和数值数组进入同一接口，省略及空列表均可采用默认配置。
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("viewport", &item);
    QQmlExpression defaults(engine.rootContext(), nullptr, "viewport.setAttriMode(7, 'f_normal_3', 1, {scalar_range: [], display_range: []})");
    const auto accepted = defaults.evaluate();
    INFO(defaults.error().toString().toStdString());
    REQUIRE_FALSE(defaults.hasError());
    REQUIRE(accepted.toBool());
    item.cancelComponentAttri(7);
    CHECK(item.scalarRangeInfo(7, "f_scalar_1") == original);
}

TEST_CASE("Scalar configs restore across source reload without stealing the shared bar", "[scalar-range][no-window]")
{
    testApplication();
    auto mesh = MakeMeshDataWithAtri();
    auto& values = mesh.face_attributes_["f_scalar_1"];
    REQUIRE(values.size() > 3);
    for (size_t id = 0; id < values.size(); ++id)
        values[id] = static_cast<double>(id);
    QRenderWindow item;
    item.loadMeshData(7, MakeMeshDataVtk(mesh));
    item.loadMeshData(8, MakeMeshDataVtk(mesh));
    REQUIRE(item.setAttriMode(7, "f_scalar_1", static_cast<int>(Mode::SCALAR), { { "scalar_range", QVariantList { -2.0, 2.0 } }, { "display_range", QVariantList { 1.0, 3.0 } } }));
    REQUIRE(item.setAttriMode(8, "f_normal_3", static_cast<int>(Mode::SCALAR), { { "scalar_range", QVariantList { 100.0, 200.0 } } }));
    // 通过唯一参数入口传递完整范围，后续检查实际映射范围和筛选单元。
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("viewport", &item);
    QQmlExpression request(engine.rootContext(), nullptr, "viewport.setAttriMode(7, 'f_scalar_1', 1, {scalar_range: [-4, 4], display_range: [1, 3]})");
    const auto accepted = request.evaluate();
    INFO(request.error().toString().toStdString());
    REQUIRE_FALSE(request.hasError());
    REQUIRE(accepted.toBool());
    // 无效请求不能替换任一组件配置，也不能改变场景重建后的共享色柱归属。
    CHECK_FALSE(item.setAttriMode(7, "f_normal_3", static_cast<int>(Mode::SCALAR), { { "scalar_range", QVariantList { 3.0, 1.0 } } }));
    CHECK_FALSE(item.setAttriMode(8, "f_scalar_1", static_cast<int>(Mode::SCALAR), { { "display_range", QVariantList { "bad", 1.0 } } }));
    // 无效请求必须先被拒绝，不能清除当前筛选或破坏共享色柱的恢复依据。
    SECTION("Invalid render modes retain the active scalar display")
    {
        for (const int invalid_mode : { -1, 4, 999 })
            CHECK_FALSE(item.setAttriMode(7, "f_scalar_1", invalid_mode, { }));
    }
    SECTION("Unknown components retain another component's scalar bar")
    {
        CHECK_FALSE(item.setAttriMode(9999, "f_normal_3", static_cast<int>(Mode::RGB), { }));
    }
    std::uint64_t previous_generation = 0;
    for (int cycle = 0; cycle < 3; ++cycle) {
        // 通用窗口不创建原生窗口或 OpenGL 上下文，关闭交互器自动渲染以避免控件触发绘制。
        vtkNew<vtkGenericOpenGLRenderWindow> window;
        window->SetReadyForRendering(false);
        window->SetSize(800, 600);
        vtkNew<vtkRenderWindowInteractor> interactor;
        interactor->EnableRenderOff();
        interactor->SetRenderWindow(window);
        auto user_data = item.initializeVTK(window);
        auto* data = QRenderWindow::Data::SafeDownCast(user_data);
        REQUIRE(data);
        auto owner = data->mesh_actor_manager_->getComponentActor(7);
        auto other = data->mesh_actor_manager_->getComponentActor(8);
        REQUIRE(owner);
        REQUIRE(other);
        AttributeOperator owner_attributes(owner.get());
        AttributeOperator other_attributes(other.get());
        auto* mapper = owner_attributes.getFaceMapper();
        mapper->Update();
        auto* ids = vtkIdTypeArray::SafeDownCast(mapper->GetInput()->GetCellData()->GetArray("PrecessFaceIds"));
        REQUIRE(ids);
        std::vector<vtkIdType> actual_ids;
        for (vtkIdType id = 0; id < ids->GetNumberOfValues(); ++id)
            actual_ids.push_back(ids->GetValue(id));
        std::vector<vtkIdType> expected_ids { 1, 2, 3 };
        if (cycle == 1)
            expected_ids = { 0, 1, 2 };
        else if (cycle == 2) {
            expected_ids.clear();
            for (size_t id = 0; id < values.size(); ++id)
                expected_ids.push_back(static_cast<vtkIdType>(id));
        }
        CHECK(actual_ids == expected_ids);
        CHECK(static_cast<bool>(owner_attributes.getFaceMapper()->GetScalarVisibility()) == (cycle < 2));
        if (cycle < 2) {
            CHECK(owner_attributes.getFaceMapper()->GetScalarRange()[0] == -4.0);
            CHECK(owner_attributes.getFaceMapper()->GetScalarRange()[1] == 4.0);
        }
        CHECK(other_attributes.getFaceMapper()->GetScalarRange()[0] == 100.0);
        CHECK(other_attributes.getFaceMapper()->GetScalarRange()[1] == 200.0);
        vtkScalarBarActor* scalar_bar = nullptr;
        auto* props = data->renderer_->GetViewProps();
        props->InitTraversal();
        while (auto* prop = props->GetNextProp()) {
            if (auto* bar = vtkScalarBarActor::SafeDownCast(prop))
                scalar_bar = bar;
        }
        REQUIRE(scalar_bar);
        CHECK(static_cast<bool>(scalar_bar->GetVisibility()) == (cycle < 2));
        if (cycle < 2)
            CHECK(scalar_bar->GetLookupTable() == owner_attributes.getFaceMapper()->GetLookupTable());
        CHECK((other->dataGeneration() != previous_generation) == (cycle < 2));
        previous_generation = other->dataGeneration();
        CHECK(window->GetGenericWindowId() == nullptr);
        item.destroyingVTK(window, user_data);
        window->RemoveRenderer(data->overlay_renderer_);
        window->RemoveRenderer(data->renderer_);
        // 自动恢复非色柱归属组件，原归属组件的配置必须仍可在下一次场景重建中恢复。
        if (cycle == 0) {
            // 改变筛选结果后重载归属组件和另一组件，验证配置恢复及色柱归属。
            for (auto& value : values)
                value += 1.0;
            item.loadMeshData(7, MakeMeshDataVtk(mesh));
            item.loadMeshData(8, MakeMeshDataVtk(mesh));
        } else if (cycle == 1)
            item.cancelComponentAttri(7);
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
