/**
 * @file DockTheme.h
 * @brief 停靠组件主题承载对象：保存 app 侧推送的已解析配色并通知 dock QML
 *
 * dock 模块刻意仅依赖 Qt、不反向依赖 app，故由本对象承载配色表；app 侧经
 * QML Binding 把 Theme 的 dockColors / controlPalette / popupPalette 推入，
 * dock QML 只读取已解析值。本对象不再保存明暗状态或计算产品配色。
 */

#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

namespace dock::ui {

/**
 * @brief 停靠组件主题单例
 *
 * 三个颜色表均由宿主推送：colors 供外壳绘制，controlPalette 供浮窗公共祖先
 * 绑定普通控件调色板，popupPalette 供菜单/弹窗使用。
 */
class DockTheme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap colors READ colors WRITE setColors NOTIFY colorsChanged)
    Q_PROPERTY(QVariantMap controlPalette READ controlPalette WRITE setControlPalette NOTIFY controlPaletteChanged)
    Q_PROPERTY(QVariantMap popupPalette READ popupPalette WRITE setPopupPalette NOTIFY popupPaletteChanged)
    //! @brief 自定义壁纸图像源（app 侧 Binding 推入；空串时浮窗显示纯色兜底）
    Q_PROPERTY(QString wallpaperUrl READ wallpaperUrl WRITE setWallpaperUrl NOTIFY wallpaperUrlChanged)

public:
    explicit DockTheme(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    /** @brief 停靠外壳绘制色（由 Theme.dockColors 推送） */
    QVariantMap colors() const { return colors_; }

    /** @brief 普通控件调色板（由 Theme.controlPalette 推送） */
    QVariantMap controlPalette() const { return control_palette_; }

    /** @brief 弹窗调色板（由 Theme.popupPalette 推送） */
    QVariantMap popupPalette() const { return popup_palette_; }

    /** @brief 自定义壁纸图像源（由 app 侧 Binding 驱动） */
    QString wallpaperUrl() const { return wallpaper_url_; }

    void setColors(const QVariantMap& colors)
    {
        if (colors_ == colors)
            return;
        colors_ = colors;
        emit colorsChanged();
    }

    void setControlPalette(const QVariantMap& palette)
    {
        if (control_palette_ == palette)
            return;
        control_palette_ = palette;
        emit controlPaletteChanged();
    }

    void setPopupPalette(const QVariantMap& palette)
    {
        if (popup_palette_ == palette)
            return;
        popup_palette_ = palette;
        emit popupPaletteChanged();
    }

    void setWallpaperUrl(const QString& url)
    {
        if (wallpaper_url_ == url)
            return;
        wallpaper_url_ = url;
        emit wallpaperUrlChanged();
    }

signals:
    void colorsChanged();
    void controlPaletteChanged();
    void popupPaletteChanged();
    void wallpaperUrlChanged();

private:
    QVariantMap colors_;
    QVariantMap control_palette_;
    QVariantMap popup_palette_;
    QString wallpaper_url_;
};

}
