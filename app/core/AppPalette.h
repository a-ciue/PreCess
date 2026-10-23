/**
 * @file AppPalette.h
 * @brief 应用级调色板单例：把 Theme 已解析的角色表应用到 QGuiApplication
 *
 * 部分弹窗（如 ToolTip）按 QGuiApplication::palette() 取色。本单例只接收
 * Theme.controlPalette 推送的已解析颜色，经 ThemePalette.h 转成 QPalette 后
 * 写入应用调色板；自身不再保存明暗/壁纸状态，也不重新计算产品配色。
 *
 * 注意：Fusion 样式的默认控件调色板来自平台主题，后创建的控件不会读取
 * 应用调色板；控件继承依赖 Main.qml 在 ApplicationWindow 上的 palette 绑定，
 * 不能以本单例替代。
 */

#pragma once

#include <QGuiApplication>
#include <QObject>
#include <QVariantMap>
#include <qqmlintegration.h>

#include "ThemePalette.h"

class AppPalette : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QVariantMap colors READ colors WRITE setColors NOTIFY colorsChanged)

public:
    explicit AppPalette(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    /** @brief 当前已解析的角色颜色表（由 Theme.controlPalette 驱动） */
    QVariantMap colors() const { return colors_; }

    /** @brief 接收主题角色表并刷新应用调色板 */
    void setColors(const QVariantMap& colors)
    {
        if (colors_ == colors)
            return;
        colors_ = colors;
        applyPalette();
        emit colorsChanged();
    }

signals:
    void colorsChanged();

private:
    //! @brief 按 Theme 已解析的角色表刷新应用调色板
    void applyPalette()
    {
        QGuiApplication::setPalette(app::core::buildThemePalette(colors_));
    }

    QVariantMap colors_;
};
