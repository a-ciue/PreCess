/**
 * @file AppPalette.h
 * @brief 应用级调色板单例：让 ToolTip 等按应用调色板取色的弹窗跟随主题
 *
 * 部分弹窗（如 ToolTip）在本应用中按 QGuiApplication::palette() 取色，
 * 而非窗口调色板；本单例把主题明暗与壁纸状态写入应用调色板，由 Main.qml
 * 绑定 Theme.dark / Theme.hasWallpaper 驱动。菜单/对话框另有显式 palette
 * 绑定，不依赖本单例。
 *
 * 注意：Fusion 样式的默认控件调色板来自平台主题，后创建的控件不会读取
 * 应用调色板；控件继承依赖 Main.qml 在 ApplicationWindow 上的 palette 绑定，
 * 不能以本单例替代。
 *
 * 调色板角色色值由 ThemePalette.h 提供（须与 app/Theme.qml 保持一致）。
 */

#pragma once

#include <QColor>
#include <QGuiApplication>
#include <QObject>
#include <QPalette>
#include <qqmlintegration.h>

#include "ThemePalette.h"

class AppPalette : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY darkChanged)
    Q_PROPERTY(bool hasWallpaper READ hasWallpaper WRITE setHasWallpaper NOTIFY hasWallpaperChanged)

public:
    explicit AppPalette(QObject* parent = nullptr)
        : QObject(parent)
    {
        applyPalette();
    }

    bool dark() const { return dark_; }
    bool hasWallpaper() const { return has_wallpaper_; }

    /** @brief 设置明暗并刷新应用调色板（由 Theme.dark 驱动） */
    void setDark(bool dark)
    {
        if (dark_ == dark)
            return;
        dark_ = dark;
        applyPalette();
        emit darkChanged();
    }

    /** @brief 设置壁纸状态并刷新应用调色板（由 Theme.hasWallpaper 驱动） */
    void setHasWallpaper(bool has_wallpaper)
    {
        if (has_wallpaper_ == has_wallpaper)
            return;
        has_wallpaper_ = has_wallpaper;
        applyPalette();
        emit hasWallpaperChanged();
    }

signals:
    void darkChanged();
    void hasWallpaperChanged();

private:
    //! @brief 按当前明暗与壁纸状态刷新应用调色板
    void applyPalette()
    {
        QGuiApplication::setPalette(app::core::buildThemePalette(dark_, has_wallpaper_));
    }

    bool dark_ = false;
    bool has_wallpaper_ = false;
};
