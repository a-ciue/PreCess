/**
 * @file AppPalette.h
 * @brief 应用级调色板单例：让 ToolTip 等按应用调色板取色的弹窗跟随主题
 *
 * 部分弹窗（如 ToolTip）在本应用中按 QGuiApplication::palette() 取色，
 * 而非窗口调色板；本单例把主题明暗写入应用调色板，由 Main.qml 绑定
 * Theme.dark 驱动。菜单/对话框另有显式 palette 绑定，不依赖本单例。
 *
 * 角色色值须与 app/Theme.qml、app/Main.qml 的调色板保持一致。
 */

#pragma once

#include <QColor>
#include <QGuiApplication>
#include <QObject>
#include <QPalette>
#include <qqmlintegration.h>

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
    //! @brief 按当前明暗组装 QPalette 并写入应用调色板
    void applyPalette()
    {
        QPalette palette;
        palette.setColor(QPalette::Window, themed("#F3F4F6", "#1E1E1E"));
        palette.setColor(QPalette::WindowText, themed("#2B2F36", "#D4D4D4"));
        palette.setColor(QPalette::Base, themed("#FFFFFF", "#252526"));
        palette.setColor(QPalette::AlternateBase, themed("#F7F8FA", "#2D2D30"));
        palette.setColor(QPalette::Text, themed("#2B2F36", "#D4D4D4"));
        palette.setColor(QPalette::Button, themed("#F7F8FA", "#2D2D30"));
        palette.setColor(QPalette::ButtonText, themed("#2B2F36", "#D4D4D4"));
        palette.setColor(QPalette::Highlight, themed("#1976D2", "#4EA1F3"));
        palette.setColor(QPalette::HighlightedText, themed("#FFFFFF", "#FFFFFF"));
        palette.setColor(QPalette::PlaceholderText, themed("#9CA3AF", "#6B6F76"));
        palette.setColor(QPalette::Mid, themed("#C3C8CF", "#4A4A4F"));
        palette.setColor(QPalette::Midlight, themed("#D9DDE3", "#3E3E42"));
        palette.setColor(QPalette::Light, themed("#FFFFFF", "#252526"));
        palette.setColor(QPalette::Dark, themed("#C3C8CF", "#4A4A4F"));
        palette.setColor(QPalette::Shadow, themed("#B7BDC7", "#5A5A5A"));
        palette.setColor(QPalette::Link, themed("#1976D2", "#4EA1F3"));
        palette.setColor(QPalette::ToolTipBase, themed("#FFFFFF", "#252526"));
        palette.setColor(QPalette::ToolTipText, themed("#2B2F36", "#D4D4D4"));
        QGuiApplication::setPalette(palette);
    }

    QColor themed(const char* light, const char* dark_hex) const
    {
        return QColor(QLatin1String(dark_ ? dark_hex : light));
    }

    bool dark_ = false;
};
