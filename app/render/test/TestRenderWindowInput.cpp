/**
 * @file TestRenderWindowInput.cpp
 * @brief 经真实 QQuickWindow、Qt 输入适配器和渲染队列验证点击与拖动。
 */
#include "QRenderWindow.h"
#include "QRenderWindowStyle.h"
#include "ViewportOrientationRepresentation.h"

#include <QGuiApplication>
#include <QMouseEvent>
#include <QQuickWindow>
#include <QTest>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <vtkCallbackCommand.h>
#include <vtkCamera.h>
#include <vtkTransform.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>

namespace {
QGuiApplication& testApplication()
{
    // 使用真实 Qt Quick/OpenGL 宿主；basic 令测试窗口的场景图清理顺序可重复。
    if (qEnvironmentVariableIsEmpty("QSG_RENDER_LOOP"))
        qputenv("QSG_RENDER_LOOP", "basic");
    QQuickVTKItem::setGraphicsApi();
    static int argc = 1;
    static char name[] = "TestRenderWindowInput";
    static char* argv[] = { name, nullptr };
    static QGuiApplication app(argc, argv);
    return app;
}

struct Observations {
    std::atomic<int> presses {};
    std::atomic<int> releases {};
    std::atomic<int> selections {};
};

class InputWindow : public QRenderWindow {
public:
    std::shared_ptr<Observations> observed = std::make_shared<Observations>();

    vtkUserData initializeVTK(vtkRenderWindow* window) override
    {
        auto data = QRenderWindow::initializeVTK(window);
        auto* vtk = Data::SafeDownCast(data);
        vtkNew<vtkCallbackCommand> observer;
        // 相机 style/widget 会 GrabFocus；被动观察者仍能记录被其独占的 release。
        observer->PassiveObserverOn();
        observer->SetClientData(observed.get());
        observer->SetCallback([](vtkObject*, unsigned long event, void* data, void*) {
            auto* counts = static_cast<Observations*>(data);
            if (event == vtkCommand::LeftButtonPressEvent)
                ++counts->presses;
            else if (event == vtkCommand::LeftButtonReleaseEvent)
                ++counts->releases;
            else if (event == vtkCommand::SelectionChangedEvent)
                ++counts->selections;
        });
        // 在 widget 截获事件之前计数，才能检测重复 press。
        window->GetInteractor()->AddObserver(vtkCommand::LeftButtonPressEvent, observer, 1.0);
        window->GetInteractor()->AddObserver(vtkCommand::LeftButtonReleaseEvent, observer, 1.0);
        vtk->style_->AddObserver(vtkCommand::SelectionChangedEvent, observer);
        return data;
    }
};

struct InputFixture {
    QQuickWindow window;
    InputWindow* item = new InputWindow;

    InputFixture()
    {
        window.resize(640, 480);
        item->setParent(window.contentItem());
        item->setParentItem(window.contentItem());
        item->setSize(QSizeF(640, 480));
        window.show();
        REQUIRE(QTest::qWaitForWindowExposed(&window));
        flush();
    }

    void flush()
    {
        auto completed = std::make_shared<std::atomic<bool>>(false);
        item->dispatch_async([completed](vtkRenderWindow*, QQuickVTKItem::vtkUserData) { *completed = true; });
        REQUIRE(QTest::qWaitFor([&] { return completed->load(); }, 5000));
    }

    std::array<double, 3> cameraDirection()
    {
        std::array<double, 3> direction {};
        item->dispatch_async([&](vtkRenderWindow*, QQuickVTKItem::vtkUserData user_data) {
            auto* vtk = QRenderWindow::Data::SafeDownCast(user_data);
            const auto* value = vtk->renderer_->GetActiveCamera()->GetDirectionOfProjection();
            std::copy_n(value, 3, direction.begin());
        });
        flush();
        return direction;
    }

    QPoint axisPoint()
    {
        QPoint point;
        const double ratio = window.devicePixelRatio();
        item->dispatch_async([&](vtkRenderWindow* render_window, QQuickVTKItem::vtkUserData user_data) {
            auto* vtk = QRenderWindow::Data::SafeDownCast(user_data);
            auto* camera = vtk->renderer_->GetActiveCamera();
            camera->SetPosition(0, 0, 1);
            camera->SetFocalPoint(0, 0, 0);
            camera->SetViewUp(0, 1, 0);
            vtk->orientationWidget->SquareResize();
            auto* orientation = ViewportOrientationRepresentation::SafeDownCast(vtk->orientationWidget->GetRepresentation());
            orientation->GetTransform()->Identity();
            orientation->BuildRepresentation();
            const auto* origin = orientation->GetRenderer()->GetOrigin();
            point = QPoint(qRound((origin[0] + 98) / ratio),
                qRound((render_window->GetSize()[1] - 1 - origin[1] - 60) / ratio));
        });
        flush();
        return point;
    }

    ~InputFixture()
    {
        window.hide();
        window.releaseResources();
        // item 由窗口内容对象持有，先拆解场景图，再销毁 QObject。
    }
};
}

TEST_CASE("QRenderWindow Qt input forwards each press once and clears click candidates")
{
    testApplication();
    InputFixture fixture;
    auto counts = fixture.item->observed;
    int clicks = 0;
    int right_clicks = 0;
    QObject::connect(fixture.item, &QRenderWindow::clicked, [&] { ++clicks; });
    QObject::connect(fixture.item, &QRenderWindow::rightClicked, [&] { ++right_clicks; });
    const QPoint model_point(200, 200);
    QTest::mouseClick(&fixture.window, Qt::LeftButton, Qt::NoModifier, model_point);
    fixture.flush();
    CHECK(counts->presses == 1);
    CHECK(counts->releases == 1);
    CHECK(counts->selections == 1);
    CHECK(clicks == 1);
    const auto before = fixture.cameraDirection();
    QTest::mouseMove(&fixture.window, QPoint(260, 240));
    fixture.flush();
    CHECK(fixture.cameraDirection() == before);
    CHECK(counts->presses == 1);

    QTest::mousePress(&fixture.window, Qt::LeftButton, Qt::NoModifier, model_point);
    QTest::mouseMove(&fixture.window, model_point + QPoint(40, 20));
    QTest::mouseRelease(&fixture.window, Qt::LeftButton, Qt::NoModifier, model_point + QPoint(40, 20));
    fixture.flush();
    CHECK(counts->presses == 2);
    CHECK(counts->releases == 2);
    CHECK(counts->selections == 1);
    CHECK(clicks == 1);
    CHECK(fixture.cameraDirection() != before);
    const auto after_drag = fixture.cameraDirection();
    QTest::mouseMove(&fixture.window, QPoint(320, 280));
    fixture.flush();
    CHECK(fixture.cameraDirection() == after_drag);
    QTest::mouseClick(&fixture.window, Qt::LeftButton, Qt::NoModifier, model_point);
    fixture.flush();
    CHECK(counts->selections == 2);
    CHECK(clicks == 2);

    QTest::mouseClick(&fixture.window, Qt::RightButton, Qt::NoModifier, model_point);
    QTest::mouseClick(&fixture.window, Qt::MiddleButton, Qt::NoModifier, model_point);
    fixture.flush();
    CHECK(right_clicks == 1);
    CHECK(clicks == 2);
    CHECK(counts->selections == 2);
}

TEST_CASE("QRenderWindow Qt input isolates orientation clicks and dragging from model selection")
{
    testApplication();
    InputFixture fixture;
    auto counts = fixture.item->observed;
    QPoint axis = fixture.axisPoint();
    QTest::mouseMove(&fixture.window, QPoint(200, 200));
    fixture.flush();
    QTest::mouseMove(&fixture.window, axis);
    fixture.flush();
    QTest::mouseClick(&fixture.window, Qt::LeftButton, Qt::NoModifier, axis);
    fixture.flush();
    CHECK(counts->selections == 0);
    CHECK(fixture.cameraDirection()[0] == Catch::Approx(-1.0));
    QTest::mouseMove(&fixture.window, QPoint(200, 200));
    QTest::mouseClick(&fixture.window, Qt::LeftButton, Qt::NoModifier, QPoint(200, 200));
    fixture.flush();
    CHECK(counts->selections == 1);

    axis = fixture.axisPoint();
    QTest::mouseMove(&fixture.window, axis);
    fixture.flush();
    QTest::mousePress(&fixture.window, Qt::LeftButton, Qt::NoModifier, axis);
    QTest::mouseMove(&fixture.window, axis + QPoint(-20, 20));
    QTest::mouseRelease(&fixture.window, Qt::LeftButton, Qt::NoModifier, axis + QPoint(-20, 20));
    fixture.flush();
    CHECK(counts->presses == 3);
    CHECK(counts->releases == 3);
    CHECK(counts->selections == 1);
    const auto after_drag = fixture.cameraDirection();
    QTest::mouseMove(&fixture.window, QPoint(200, 200));
    fixture.flush();
    CHECK(fixture.cameraDirection() == after_drag);
    axis = fixture.axisPoint();
    QTest::mouseMove(&fixture.window, axis);
    fixture.flush();
    QTest::mouseClick(&fixture.window, Qt::LeftButton, Qt::NoModifier, axis);
    fixture.flush();
    CHECK(fixture.cameraDirection()[0] == Catch::Approx(-1.0));
    CHECK(counts->selections == 1);
}

TEST_CASE("QRenderWindow Qt input checks release distance even without a mouse move")
{
    testApplication();
    InputFixture fixture;
    int clicks = 0;
    QObject::connect(fixture.item, &QRenderWindow::clicked, [&] { ++clicks; });
    // 直接投递 Qt 事件以保证没有平台合成的中间 move，仍经过真实 VTK 渲染队列。
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(200, 200), QPointF(200, 200),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(250, 250), QPointF(250, 250),
        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(fixture.item, &press);
    QCoreApplication::sendEvent(fixture.item, &release);
    fixture.flush();
    CHECK(clicks == 0);
    CHECK(fixture.item->observed->presses == 1);
    CHECK(fixture.item->observed->releases == 1);
    CHECK(fixture.item->observed->selections == 0);
}

TEST_CASE("QRenderWindow Qt input uses the current release position at the orientation boundary")
{
    testApplication();
    InputFixture fixture;
    const QPoint axis = fixture.axisPoint();
    const double ratio = fixture.window.devicePixelRatio();
    const QPoint outside = axis + QPoint(qRound(18 / ratio), 0);
    const QPoint inside = axis + QPoint(qRound(14 / ratio), 0);
    QTest::mouseMove(&fixture.window, outside);
    fixture.flush();
    // 4 个物理像素内跨入圆盘，release 之前的 VTK 坐标仍在圆盘外。
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(outside), QPointF(outside),
        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(inside), QPointF(inside),
        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(fixture.item, &press);
    QCoreApplication::sendEvent(fixture.item, &release);
    fixture.flush();
    CHECK(fixture.item->observed->selections == 0);
    QTest::mouseMove(&fixture.window, QPoint(200, 200));
    QTest::mouseClick(&fixture.window, Qt::LeftButton, Qt::NoModifier, QPoint(200, 200));
    fixture.flush();
    CHECK(fixture.item->observed->selections == 1);
}

TEST_CASE("QRenderWindow Qt input preserves control click and rectangle selection")
{
    testApplication();
    InputFixture fixture;
    QTest::mouseClick(&fixture.window, Qt::LeftButton, Qt::ControlModifier, QPoint(200, 200));
    fixture.flush();
    CHECK(fixture.item->observed->selections == 1);
    QTest::mousePress(&fixture.window, Qt::LeftButton, Qt::ControlModifier, QPoint(200, 200));
    QTest::mouseMove(&fixture.window, QPoint(250, 240));
    QTest::mouseRelease(&fixture.window, Qt::LeftButton, Qt::ControlModifier, QPoint(250, 240));
    fixture.flush();
    CHECK(fixture.item->observed->selections == 2);
    CHECK(fixture.item->observed->presses == 2);
    CHECK(fixture.item->observed->releases == 2);
}

TEST_CASE("QRenderWindow safely destroys the viewport during a held mouse press")
{
    testApplication();
    {
        InputFixture fixture;
        QTest::mousePress(&fixture.window, Qt::LeftButton, Qt::NoModifier, QPoint(200, 200));
        fixture.flush();
        CHECK(fixture.item->observed->presses == 1);
        CHECK(fixture.item->observed->releases == 0);
        // 不发送 release，覆盖 style 尚未释放事件焦点时的真实场景图销毁。
    }
    // 被测窗口已销毁；清空 QtTest 的合成按键状态，避免影响同进程内的其他用例。
    QWindow release_window;
    release_window.resize(640, 480);
    QTest::mouseRelease(&release_window, Qt::LeftButton, Qt::NoModifier, QPoint(200, 200));
}
