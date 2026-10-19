/** @file TestBusyMask.cpp
 * @brief 主窗口忙碌遮罩的鼠标穿透回归测试
 */
#include <QFile>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWheelEvent>
#include <catch2/catch_test_macros.hpp>
#include <memory>

namespace {
// 与 VTK 一样接收原生 hover 事件，不以 MouseArea 模拟渲染控件。
class QHoverProbe : public QQuickItem {
    Q_OBJECT
public:
    explicit QHoverProbe(QQuickItem* parent = nullptr)
        : QQuickItem(parent)
    {
        setAcceptHoverEvents(true);
    }
    int hoverMoves() const { return hover_moves_; }

protected:
    void hoverMoveEvent(QHoverEvent* event) override
    {
        ++hover_moves_;
        QQuickItem::hoverMoveEvent(event);
    }

private:
    int hover_moves_ { 0 };
};
QGuiApplication& testApplication()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    static int argc = 1;
    static char name[] = "TestBusyMask";
    static char* argv[] = { name, nullptr };
    static QGuiApplication app(argc, argv);
    return app;
}
void clickWindow(QQuickWindow& window, QPointF position, Qt::MouseButton button)
{
    const QPointF global = window.mapToGlobal(position.toPoint());
    QMouseEvent press(QEvent::MouseButtonPress, position, global, button, button, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, position, global, button, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &release);
}
void moveWindow(QQuickWindow& window, QPointF position)
{
    const QPointF global = window.mapToGlobal(position.toPoint());
    QMouseEvent move(QEvent::MouseMove, position, global, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &move);
}
void scrollWindow(QQuickWindow& window, QPointF position)
{
    const QPointF global = window.mapToGlobal(position.toPoint());
    QWheelEvent wheel(position, global, QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(&window, &wheel);
}
}

TEST_CASE("Busy mask blocks mouse buttons, scrolling and native hover", "[QML][busy]")
{
    testApplication();
    static const int probe_type = qmlRegisterType<QHoverProbe>("BusyMaskTest", 1, 0, "HoverProbe");
    QFile main_qml(":/busy-mask/Main.qml");
    REQUIRE(main_qml.open(QIODevice::ReadOnly));
    QByteArray source = main_qml.readAll().replace("\r\n", "\n");
    const auto start = source.indexOf("    Rectangle {\n        id: busyMask");
    REQUIRE(start >= 0);
    const auto end = source.indexOf("\n    Component.onCompleted:", start);
    REQUIRE(end > start);
    // 使用生产遮罩原文，隔离模型与渲染依赖；header/footer 保留真实布局语义。
    const QByteArray fixture = R"QML(
        import QtQuick
        import QtQuick.Controls
        import BusyMaskTest
        ApplicationWindow {
            id: root
            width: 400
            height: 300
            visible: true
            property bool frozenBusy: false
            property int clicks: 0
            property int scrolls: 0
            property int footerClicks: 0
            property int hoverMoves: 0
            header: Item { height: 40 }
            footer: MouseArea {
                height: 30
                onClicked: root.footerClicks++
            }
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.AllButtons
                onClicked: root.clicks++
                onWheel: (wheel) => { root.scrolls++; wheel.accepted = true; }
            }
            HoverProbe {
                objectName: "hoverProbe"
                anchors.fill: parent
                HoverHandler { onPointChanged: root.hoverMoves++ }
            }
    )QML"
        + source.mid(start, end - start) + "\n}";
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(fixture, QUrl());
    INFO(component.errorString().toStdString());
    std::unique_ptr<QObject> object(component.create());
    REQUIRE(object);
    auto* window = qobject_cast<QQuickWindow*>(object.get());
    REQUIRE(window);
    QCoreApplication::processEvents();
    auto* probe = object->findChild<QHoverProbe*>("hoverProbe");
    REQUIRE(probe);
    const QPointF center(200, 150);
    clickWindow(*window, center, Qt::LeftButton);
    clickWindow(*window, center, Qt::RightButton);
    clickWindow(*window, center, Qt::MiddleButton);
    scrollWindow(*window, center);
    REQUIRE(object->property("clicks").toInt() == 3);
    REQUIRE(object->property("scrolls").toInt() == 1);
    moveWindow(*window, QPointF(100, 100));
    moveWindow(*window, center);
    REQUIRE(object->property("hoverMoves").toInt() > 0);
    REQUIRE(probe->hoverMoves() > 0);

    object->setProperty("frozenBusy", true);
    QCoreApplication::processEvents();
    auto* content = object->property("contentItem").value<QQuickItem*>();
    REQUIRE(content);
    const int hover_moves = object->property("hoverMoves").toInt();
    const int native_hover_moves = probe->hoverMoves();
    // 下方内容不能通过遮罩中心或上下边缘接收输入。
    for (const QPointF point : { center, QPointF(5, 45), QPointF(395, 265) }) {
        clickWindow(*window, point, Qt::LeftButton);
        clickWindow(*window, point, Qt::RightButton);
        clickWindow(*window, point, Qt::MiddleButton);
        scrollWindow(*window, point);
        moveWindow(*window, point);
    }
    CHECK(object->property("hoverMoves").toInt() == hover_moves);
    CHECK(probe->hoverMoves() == native_hover_moves);
    CHECK(object->property("clicks").toInt() == 3);
    CHECK(object->property("scrolls").toInt() == 1);
    // 状态栏不在遮罩内，取消按钮所在区域继续接收点击。
    clickWindow(*window, QPointF(200, 285), Qt::LeftButton);
    CHECK(object->property("footerClicks").toInt() == 1);
    object->setProperty("frozenBusy", false);
    clickWindow(*window, center, Qt::LeftButton);
    CHECK(object->property("clicks").toInt() == 4);
    moveWindow(*window, QPointF(100, 100));
    QCoreApplication::processEvents();
    moveWindow(*window, QPointF(120, 120));
    CHECK(object->property("hoverMoves").toInt() > hover_moves);
    CHECK(probe->hoverMoves() > native_hover_moves);
}

#include "TestBusyMask.moc"
