/**
 * @file QApplicationStartup.h
 * @brief 装配主界面并管理应用启动期间的窗口和回调。
 */

#pragma once

#include <QObject>
#include <QPointer>

#include <memory>

class QGuiApplication;
class QQmlApplicationEngine;
class QQuickWindow;
class QStartupSplashWindow;

//! @brief 由主程序持有的启动协调对象，生命周期覆盖 QML 引擎。
class QApplicationStartup final : public QObject {
public:
    explicit QApplicationStartup(QGuiApplication& app);
    ~QApplicationStartup() override;

    //! @brief 启动桌面事件循环，或为浏览器完成同步初始化，返回退出码。
    int run();

private:
    bool initialize();
    void installKeyEventForwarder();
    void failStartup(const QString& message);

#ifndef __EMSCRIPTEN__
    bool connectStartupReadiness();
    void waitForPresentedFrame();
    void finishStartup();
#endif

    QGuiApplication& app_;
    // 引擎拆解时仍可能投递渲染通知，终态及 QObject 上下文须晚于引擎析构。
    bool startup_finished_ { false };
#ifndef __EMSCRIPTEN__
    std::unique_ptr<QStartupSplashWindow> splash_;
    QPointer<QQuickWindow> main_window_;
    QMetaObject::Connection startup_error_connection_;
#endif
    std::unique_ptr<QQmlApplicationEngine> engine_;
    // 过滤器引用引擎中的功能适配器，须先于引擎析构。
    std::unique_ptr<QObject> key_event_forwarder_;
};
