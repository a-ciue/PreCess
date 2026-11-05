/**
 * @file TestWallpaperBackground.cpp
 * @brief 壁纸背景组件行为测试：A→B 淡入、淡入中清除、加载失败回退
 */
#include <QColor>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QImage>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QQuickWindow>
#include <QThread>
#include <QUrl>
#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <memory>

namespace {
QGuiApplication& testApplication()
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    static int argc = 1;
    static char name[] = "TestWallpaperBackground";
    static char* argv[] = { name, nullptr };
    static QGuiApplication app(argc, argv);
    return app;
}

bool waitFor(const std::function<bool()>& predicate, int timeout_ms = 3000)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeout_ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    return predicate();
}

class TestImageProvider : public QQuickImageProvider {
public:
    TestImageProvider()
        : QQuickImageProvider(QQuickImageProvider::Image)
    {
    }

    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override
    {
        if (id == QLatin1String("missing"))
            return {};
        const int side = requestedSize.isValid() && !requestedSize.isEmpty()
            ? qMax(1, qMin(requestedSize.width(), requestedSize.height()))
            : 16;
        QImage image(side, side, QImage::Format_RGB32);
        image.fill(id == QLatin1String("second") ? Qt::green : Qt::red);
        if (size)
            *size = image.size();
        return image;
    }
};
}

TEST_CASE("WallpaperBackground hands off, clears and falls back", "[QML][Wallpaper]")
{
    testApplication();
    QQmlEngine engine;
    engine.addImageProvider(QStringLiteral("test"), new TestImageProvider);

    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/wallpaper-bg/WallpaperBackground.qml")));
    INFO(component.errorString().toStdString());
    REQUIRE(component.isReady());
    std::unique_ptr<QObject> object(component.create());
    REQUIRE(object);
    auto* background = qobject_cast<QQuickItem*>(object.get());
    REQUIRE(background);

    QQuickWindow window;
    window.resize(100, 100);
    background->setParentItem(window.contentItem());
    background->setWidth(100);
    background->setHeight(100);
    background->setProperty("backgroundColor", QColor("#010203"));
    background->setProperty("tintColor", QColor("#112233"));
    background->setProperty("fadeDuration", 0);
    window.show();

    auto* current = background->findChild<QQuickItem*>(QStringLiteral("wallpaperCurrent"));
    auto* incoming = background->findChild<QQuickItem*>(QStringLiteral("wallpaperIncoming"));
    auto* tint = background->findChild<QQuickItem*>(QStringLiteral("wallpaperTint"));
    REQUIRE(current);
    REQUIRE(incoming);
    REQUIRE(tint);

    const QUrl first(QStringLiteral("image://test/first"));
    const QUrl second(QStringLiteral("image://test/second"));

    // 初次加载：直接显示
    background->setProperty("source", first);
    REQUIRE(waitFor([&] { return current->property("status").toInt() == 1; }));
    REQUIRE(current->property("source").toUrl() == first);
    REQUIRE(tint->property("visible").toBool());

    // A → B：B 就绪后淡入并交给 current
    background->setProperty("source", second);
    REQUIRE(waitFor([&] { return current->property("source").toUrl() == second; }));
    REQUIRE(incoming->property("source").toUrl().isEmpty());

    // 淡入中清除：停止动画并回到纯色
    background->setProperty("fadeDuration", 500);
    background->setProperty("source", first);
    QCoreApplication::processEvents();
    background->setProperty("source", QUrl());
    // 清除时遮罩同步隐藏，不能留下 300ms 的过渡残影
    CHECK_FALSE(tint->property("visible").toBool());
    REQUIRE(waitFor([&] {
        return current->property("source").toUrl().isEmpty()
            && incoming->property("source").toUrl().isEmpty();
    }));
    REQUIRE_FALSE(background->property("failed").toBool());
    REQUIRE_FALSE(tint->property("visible").toBool());

    // 选择的图加载失败：清空两张图并显示纯色兜底
    background->setProperty("source", QUrl(QStringLiteral("image://test/missing")));
    REQUIRE(waitFor([&] { return background->property("failed").toBool(); }));
    REQUIRE(current->property("source").toUrl().isEmpty());
    REQUIRE(incoming->property("source").toUrl().isEmpty());
    REQUIRE_FALSE(tint->property("visible").toBool());
}
