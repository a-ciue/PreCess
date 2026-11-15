/**
 * @file QStartupSplashWindow.h
 * @brief 不依赖主 QML 和三维场景的轻量启动窗口
 */
#pragma once

#include <QBackingStore>
#include <QImage>
#include <QTimer>
#include <QWindow>

class QPainter;

//! @brief 独立绘制启动封面与活动动画，不在插件构造中重入事件循环
class QStartupSplashWindow final : public QWindow {
    Q_OBJECT

public:
    //! @brief 背景不可用时沿用同一窗口的纯色绘制；路径可指定替代资源。
    explicit QStartupSplashWindow(const QString& version,
        const QString& background_path = QStringLiteral(":/images/splash/ZenithGrid-splash-background@2x.png"));

signals:
    //! @brief 启动页第一次绘制并提交后，允许排队开始主程序初始化
    void onFirstFrame();
    //! @brief 用户关闭启动窗口时取消本次启动
    void onCancelled();

protected:
    bool event(QEvent* event) override;
    void exposeEvent(QExposeEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void renderNow();
    void paintContents(QPainter& painter) const;

    QBackingStore backing_store_;
    QImage background_;
    QTimer activity_timer_;
    QString version_;
    int activity_phase_;
    bool first_frame_ = false;
};
