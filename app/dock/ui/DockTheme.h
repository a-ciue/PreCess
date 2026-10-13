/**
 * @file DockTheme.h
 * @brief 停靠组件主题承载对象：向 dock QML 暴露当前明暗
 */

#pragma once

#include <QObject>

namespace dock::ui {

/**
 * @brief 停靠组件主题单例
 *
 * dock 模块刻意仅依赖 Qt、不反向依赖 app，故由本对象承载明暗标志；
 * app 侧经 QML Binding 把 Theme.dark 推入，dock QML 据此选择局部配色。
 */
class DockTheme : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY darkChanged)

public:
    explicit DockTheme(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    bool dark() const { return dark_; }

    /** @brief 设置明暗（由 app 侧 Binding 驱动） */
    void setDark(bool dark)
    {
        if (dark_ == dark)
            return;
        dark_ = dark;
        emit darkChanged();
    }

signals:
    void darkChanged();

private:
    bool dark_ = false;
};

}
