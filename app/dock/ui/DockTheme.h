/**
 * @file DockTheme.h
 * @brief 停靠组件主题承载对象：向 dock QML 暴露当前明暗与已解析配色
 *
 * dock 模块刻意仅依赖 Qt、不反向依赖 app，故由本对象承载明暗标志与配色表；
 * app 侧经 QML Binding 把 Theme.dark 推入，dock QML 读取 colors 中的已解析值。
 * 色值须与 app/Theme.qml 对应令牌保持一致。
 */

#pragma once

#include <QColor>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace dock::ui {

/**
 * @brief 停靠组件主题单例
 *
 * colors() 返回当前明暗下已解析的颜色表（键见实现），dark 变化时整体刷新；
 * 从而避免各 dock QML 各自复写浅/深色对。
 */
class DockTheme : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY darkChanged)
    Q_PROPERTY(QVariantMap colors READ colors NOTIFY colorsChanged)
    //! @brief 自定义壁纸图像源（app 侧 Binding 推入；空串时浮窗显示纯色兜底）
    Q_PROPERTY(QString wallpaperUrl READ wallpaperUrl WRITE setWallpaperUrl NOTIFY wallpaperUrlChanged)

public:
    explicit DockTheme(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    bool dark() const { return dark_; }

    /** @brief 自定义壁纸图像源（由 app 侧 Binding 驱动） */
    QString wallpaperUrl() const { return wallpaper_url_; }

    /** @brief 设置壁纸图像源 */
    void setWallpaperUrl(const QString& url)
    {
        if (wallpaper_url_ == url)
            return;
        wallpaper_url_ = url;
        emit wallpaperUrlChanged();
    }

    /** @brief 设置明暗（由 app 侧 Binding 驱动） */
    void setDark(bool dark)
    {
        if (dark_ == dark)
            return;
        dark_ = dark;
        colors_dirty_ = true;
        emit darkChanged();
        emit colorsChanged();
    }

    /** @brief 当前明暗下的颜色表（惰性缓存；须与 app/Theme.qml 对应令牌同步） */
    QVariantMap colors() const
    {
        if (!colors_dirty_)
            return colors_cache_;

        const bool d = dark_;
        auto c = [d](const char* light, const char* dark_hex) {
            return QColor(QLatin1String(d ? dark_hex : light));
        };
        // 半透明面令牌（与 app/Theme.qml、ThemePalette.h 同步）
        auto ca = [d](const char* light, const char* dark_hex, qreal alpha) {
            QColor color(QLatin1String(d ? dark_hex : light));
            color.setAlphaF(alpha);
            return color;
        };

        QVariantMap m;
        // 语义色（对齐 Theme.qml）
        m["windowBackground"] = c("#F3F4F6", "#1E1E1E");
        m["surface"] = ca("#FFFFFF", "#252526", 0.72);
        m["surfaceAlt"] = ca("#F7F8FA", "#2D2D30", 0.62);
        m["panelHeaderSurface"] = ca("#E6EDF5", "#333336", 0.70);
        // 菜单 / 弹窗：不透明，保证弹窗文字在壁纸上可读
        m["menuSurface"] = c("#FFFFFF", "#252526");
        m["border"] = c("#D9DDE3", "#3E3E42");
        m["borderStrong"] = c("#C3C8CF", "#4A4A4F");
        m["textPrimary"] = c("#2B2F36", "#D4D4D4");
        m["textDisabled"] = c("#9CA3AF", "#6B6F76");
        m["textOnPrimary"] = c("#FFFFFF", "#FFFFFF");
        m["primary"] = c("#1976D2", "#4EA1F3");
        m["danger"] = c("#D32F2F", "#F14C4C");
        m["scrollBarHover"] = c("#B7BDC7", "#5A5A5A");
        // 停靠外壳专用
        m["chromeIcon"] = c("#5A616B", "#9DA1A6");
        m["chromeHover"] = c("#E2E6EB", "#3A3A3D");
        m["dividerHover"] = c("#7aa7d9", "#5A87C0");
        m["dropAccent"] = c("#2f6fb5", "#4EA1F3");
        m["dropZoneFill"] = c("#cfe0f3", "#2A3A4D");
        m["dropZoneBorder"] = c("#8fb2d9", "#3E5C7D");
        m["dropZoneMarker"] = c("#9fb9d6", "#6E86A3");
        // 活跃落点前景/边框（白，刻意两主题同值：在蓝色强调填充上保持对比）
        m["dropZoneActiveFore"] = c("#FFFFFF", "#FFFFFF");
        colors_cache_ = m;
        colors_dirty_ = false;
        return colors_cache_;
    }

signals:
    void darkChanged();
    void colorsChanged();
    void wallpaperUrlChanged();

private:
    bool dark_ = false;
    QString wallpaper_url_;
    mutable QVariantMap colors_cache_;
    mutable bool colors_dirty_ = true;
};

}
