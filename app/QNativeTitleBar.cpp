/**
 * @file QNativeTitleBar.cpp
 * @brief 原生标题栏颜色与系统高对比度回退。
 */
#include "QNativeTitleBar.h"

#include <QColor>
#include <QCoreApplication>
#include <QEvent>
#include <QMetaObject>
#include <QPlatformSurfaceEvent>
#include <QVariant>
#include <QWindow>
#include <spdlog/spdlog.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
// Windows 类型必须先于 DWM 头文件声明。
// clang-format off
#include <windows.h>
#include <dwmapi.h>
// clang-format on

QNativeTitleBar::QNativeTitleBar(QWindow* window, QObject* parent)
    : QObject(parent)
    , window_(window)
    , surface_available_(window && window->handle())
{
    if (!window_)
        return;

    // 主窗口由 QML 创建，因此通过其属性变更信号同步原生颜色。
    connect(window_, SIGNAL(nativeTitleBarBackgroundChanged()), this, SLOT(refresh()));
    connect(window_, SIGNAL(nativeTitleBarTextColorChanged()), this, SLOT(refresh()));
    window_->installEventFilter(this);
    if (auto* app = QCoreApplication::instance())
        app->installNativeEventFilter(this);
    refresh();
}

QNativeTitleBar::~QNativeTitleBar()
{
    if (auto* app = QCoreApplication::instance())
        app->removeNativeEventFilter(this);
    if (window_)
        window_->removeEventFilter(this);
}

void QNativeTitleBar::refresh()
{
    // 不在窗口未创建或销毁期间调用 winId()，避免意外创建新的平台窗口。
    if (!window_ || !surface_available_ || !window_->handle())
        return;

    const auto hwnd = reinterpret_cast<HWND>(window_->winId());
    if (!hwnd)
        return;

    // 属性值从 Windows 11 22000 开始可用；数字常量使旧 Windows SDK 也可编译。
    constexpr DWORD caption_attribute = 35; // DWMWA_CAPTION_COLOR
    constexpr DWORD text_attribute = 36; // DWMWA_TEXT_COLOR
    constexpr COLORREF system_default = 0xffffffff; // DWMWA_COLOR_DEFAULT
    const auto write_colors = [hwnd](COLORREF caption_color, COLORREF text_color, const char* action) {
        const auto caption_result = DwmSetWindowAttribute(hwnd, caption_attribute, &caption_color, sizeof(caption_color));
        const auto text_result = DwmSetWindowAttribute(hwnd, text_attribute, &text_color, sizeof(text_color));
        if (SUCCEEDED(caption_result) && SUCCEEDED(text_result))
            return true;
        spdlog::debug("Failed to {} native title bar colors: caption HRESULT=0x{:08X}, text HRESULT=0x{:08X}",
            action, static_cast<unsigned long>(caption_result), static_cast<unsigned long>(text_result));
        return false;
    };

    HIGHCONTRASTW contrast {};
    contrast.cbSize = sizeof(contrast);
    if (!SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0)) {
        const auto error = GetLastError();
        spdlog::warn("Failed to query high contrast state; using system title bar colors: Win32 error={}", error);
    } else if (!(contrast.dwFlags & HCF_HIGHCONTRASTON)) {
        const QColor background = window_->property("nativeTitleBarBackground").value<QColor>();
        const QColor text = window_->property("nativeTitleBarTextColor").value<QColor>();
        if (background.isValid() && text.isValid()
            && write_colors(RGB(background.red(), background.green(), background.blue()),
                RGB(text.red(), text.green(), text.blue()), "apply"))
            return;
    }
    // 系统高对比度或写入未全部成功时，成对恢复系统配色。
    write_colors(system_default, system_default, "restore");
}

bool QNativeTitleBar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == window_ && event->type() == QEvent::PlatformSurface) {
        const auto* surface_event = static_cast<QPlatformSurfaceEvent*>(event);
        if (surface_event->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated) {
            surface_available_ = true;
            // 事件处理完毕后再取句柄，避免与平台窗口创建顺序竞争。
            QMetaObject::invokeMethod(this, &QNativeTitleBar::refresh, Qt::QueuedConnection);
        } else if (surface_event->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
            surface_available_ = false;
        }
    }
    return QObject::eventFilter(watched, event);
}

bool QNativeTitleBar::nativeEventFilter(const QByteArray& event_type, void* message, qintptr* result)
{
    Q_UNUSED(event_type);
    Q_UNUSED(result);
    const auto* msg = static_cast<const MSG*>(message);
    // 高对比度和系统主题是全局状态；不拦截通知，等待系统处理完后刷新。
    if (msg && (msg->message == WM_SETTINGCHANGE || msg->message == WM_THEMECHANGED))
        QMetaObject::invokeMethod(this, &QNativeTitleBar::refresh, Qt::QueuedConnection);
    return false;
}
