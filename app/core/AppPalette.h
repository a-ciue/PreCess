/**
 * @file AppPalette.h
 * @brief 应用级调色板单例：让 ToolTip 等按应用调色板取色的弹窗跟随主题
 *
 * 部分弹窗（如 ToolTip）在本应用中按 QGuiApplication::palette() 取色，
 * 而非窗口调色板；本单例把主题明暗写入应用调色板，由 Main.qml 绑定
 * Theme.dark 驱动。菜单/对话框另有显式 palette 绑定，不依赖本单例。
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

class AppPalette : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY darkChanged)

public:
    explicit AppPalette(QObject* parent = nullptr)
        : QObject(parent)
    {
        applyPalette();
    }

    bool dark() const { return dark_; }

    /** @brief 设置明暗并刷新应用调色板（由 Theme.dark 驱动） */
    void setDark(bool dark)
    {
        if (dark_ == dark)
            return;
        dark_ = dark;
        applyPalette();
        emit darkChanged();
    }

signals:
    void darkChanged();

private:
    //! @brief 按当前明暗刷新应用调色板
    void applyPalette()
    {
        QGuiApplication::setPalette(app::core::buildThemePalette(dark_));
    }

    bool dark_ = false;
};
