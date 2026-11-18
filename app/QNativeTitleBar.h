/**
 * @file QNativeTitleBar.h
 * @brief 跟随主窗口主题更新原生标题栏，并保留系统窗口行为。
 */
#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPointer>

class QEvent;
class QWindow;

/**
 * @brief 将主窗口的主题色同步到支持的平台原生标题栏。
 *
 * 标题栏继续由操作系统绘制。窗口属性由主界面提供；不支持颜色定制时保持系统样式。
 */
class QNativeTitleBar : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    /**
     * @brief 绑定主窗口并跟踪原生窗口和系统主题变化。
     * @param window 需要同步标题栏颜色的主窗口，不可为空。
     * @param parent 对象所有者。
     */
    explicit QNativeTitleBar(QWindow* window, QObject* parent = nullptr);
    ~QNativeTitleBar() override;

public slots:
    //! @brief 按当前窗口主题与系统无障碍状态刷新标题栏。
    void refresh();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool nativeEventFilter(const QByteArray& event_type, void* message, qintptr* result) override;

private:
    QPointer<QWindow> window_;
    bool surface_available_ = false;
};
