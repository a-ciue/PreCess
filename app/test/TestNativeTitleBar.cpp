/**
 * @file TestNativeTitleBar.cpp
 * @brief 标题栏颜色刷新不应隐式创建或重建平台窗口。
 */
#include "QNativeTitleBar.h"

#include <QColor>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QWindow>
#include <catch2/catch_test_macros.hpp>

namespace {

QGuiApplication& testApplication()
{
    static int argc = 1;
    static char name[] = "TestNativeTitleBar";
    static char* argv[] = { name, nullptr };
    static QGuiApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    return app;
}

class TestColorWindow : public QWindow {
    Q_OBJECT
    Q_PROPERTY(QColor nativeTitleBarBackground READ background WRITE setBackground NOTIFY nativeTitleBarBackgroundChanged)
    Q_PROPERTY(QColor nativeTitleBarTextColor READ textColor WRITE setTextColor NOTIFY nativeTitleBarTextColorChanged)

public:
    QColor background() const { return background_; }
    QColor textColor() const { return text_color_; }

    void setBackground(const QColor& color)
    {
        if (background_ == color)
            return;
        background_ = color;
        emit nativeTitleBarBackgroundChanged();
    }

    void setTextColor(const QColor& color)
    {
        if (text_color_ == color)
            return;
        text_color_ = color;
        emit nativeTitleBarTextColorChanged();
    }

signals:
    void nativeTitleBarBackgroundChanged();
    void nativeTitleBarTextColorChanged();

private:
    QColor background_ { QStringLiteral("#102B33") };
    QColor text_color_ { QStringLiteral("#DDE8E9") };
};

}

TEST_CASE("Native title bar updates do not create a platform surface", "[titlebar]")
{
    testApplication();
    TestColorWindow window;
    QNativeTitleBar title_bar(&window);

    REQUIRE(window.handle() == nullptr);
    window.setBackground(QColor("#F3F4F6"));
    window.setTextColor(QColor("#2B2F36"));
    title_bar.refresh();
    REQUIRE(window.handle() == nullptr);

    window.create();
    QCoreApplication::processEvents();
    REQUIRE(window.handle() != nullptr);
    window.destroy();
    REQUIRE(window.handle() == nullptr);
    window.setBackground(QColor("#102B33"));
    title_bar.refresh();
    QCoreApplication::processEvents();
    REQUIRE(window.handle() == nullptr);
}

#include "TestNativeTitleBar.moc"
