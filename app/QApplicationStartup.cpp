/**
 * @file QApplicationStartup.cpp
 * @brief 管理主界面加载、启动顺序和窗口交接。
 */

#include "QApplicationStartup.h"

#include "Docking.h"
#include "QFeatureSystemAdaptor.h"
#include "QLogManager.h"
#include "QModelManager.h"
#include "WallpaperLibrary.h"
#ifdef __EMSCRIPTEN__
#include "QWasmBridge.h"
#include <QFontDatabase>
#else
#include "QStartupSplashWindow.h"
#include <QQuickItem>
#include <QQuickVTKItem.h>
#include <QQuickWindow>
#endif

#include <QGuiApplication>
#include <QKeyEvent>
#include <QUrl>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>

#include <exception>
#include <spdlog/spdlog.h>

namespace {

//! @brief 按键由功能消费时阻止继续向焦点控件传递。
class KeyEventForwarder : public QObject {
public:
    explicit KeyEventForwarder(systems::feature::QFeatureSystemAdaptor& adaptor)
        : adaptor_(adaptor)
    {
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
            const auto* key_event = static_cast<QKeyEvent*>(event);
            if (!key_event->isAutoRepeat()
                && adaptor_.postKeyEvent(key_event->key(), static_cast<int>(key_event->modifiers()),
                    event->type() == QEvent::KeyPress)) {
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    systems::feature::QFeatureSystemAdaptor& adaptor_;
};

#ifndef __EMSCRIPTEN__
//! @brief 停靠中央区同步创建，定位视口以区分可用界面与空窗口。
QQuickVTKItem* findViewport(QQuickItem* item)
{
    if (auto* viewport = qobject_cast<QQuickVTKItem*>(item))
        return viewport;
    for (auto* child : item->childItems()) {
        if (auto* viewport = findViewport(child))
            return viewport;
    }
    return nullptr;
}
#endif

} // namespace

QApplicationStartup::QApplicationStartup(QGuiApplication& app)
    : app_(app)
{
}

QApplicationStartup::~QApplicationStartup() = default;

int QApplicationStartup::run()
{
#ifdef __EMSCRIPTEN__
    // Web 平台需在加载 QML 前手动加载捆绑字体，以显示中文。
    const int font_id = QFontDatabase::addApplicationFont(":/fonts/appfont.bin");
    const QStringList font_families = QFontDatabase::applicationFontFamilies(font_id);
    if (!font_families.isEmpty())
        app_.setFont(QFont(font_families.first()));

    // 浏览器管理事件循环，启动对象及其引擎由 main 持久保留。
    if (!initialize())
        return -1;
    installKeyEventForwarder();
    startup_finished_ = true;
    return 0;
#else
    splash_ = std::make_unique<QStartupSplashWindow>(QCoreApplication::applicationVersion());
    app_.setQuitOnLastWindowClosed(false);
    connect(splash_.get(), &QStartupSplashWindow::onCancelled, this, [this]() {
        if (startup_finished_)
            return;
        startup_finished_ = true;
        app_.quit();
    });
    connect(&app_, &QCoreApplication::aboutToQuit, this, [this]() { startup_finished_ = true; });
    connect(&app_, &QCoreApplication::aboutToQuit, splash_.get(), &QWindow::hide);

    // 先提交启动封面，再排队初始化，避免在插件构造中重入事件循环。
    connect(splash_.get(), &QStartupSplashWindow::onFirstFrame, this, [this]() {
        if (startup_finished_ || !splash_ || !splash_->isVisible())
            return;
        if (initialize())
            connectStartupReadiness(); }, Qt::QueuedConnection);
    splash_->show();
    return app_.exec();
#endif
}

bool QApplicationStartup::initialize()
{
    try {
        QLogManager::initialize();
        engine_ = std::make_unique<QQmlApplicationEngine>();
        dock::init(engine_.get());
        engine_->addImageProvider(QStringLiteral("wallpaper"), new WallpaperProvider);

        QList<QUrl> arguments;
        for (const auto& argument : app_.arguments().mid(1))
            arguments << QUrl::fromLocalFile(argument);
        engine_->rootContext()->setContextProperty("QLogManager", QLogManager::instance());
        engine_->rootContext()->setContextProperty("commandLineArgs", QVariant::fromValue(arguments));
#ifdef __EMSCRIPTEN__
        engine_->rootContext()->setContextProperty("QWasmBridge", new QWasmBridge(&app_));
#endif
        engine_->loadFromModule("app", "Main");
        if (engine_->rootObjects().isEmpty()) {
            failStartup(QCoreApplication::translate("Startup", "主界面加载失败"));
            return false;
        }

        return true;
    } catch (const std::exception& error) {
        failStartup(QString::fromUtf8(error.what()));
        return false;
    }
}

void QApplicationStartup::installKeyEventForwarder()
{
    if (auto* model_manager = engine_->singletonInstance<QModelManager*>("app.model", "QModelManager")) {
        if (auto* feature_adaptor = model_manager->getFeatureSystemAdaptor()) {
            key_event_forwarder_ = std::make_unique<KeyEventForwarder>(*feature_adaptor);
            app_.installEventFilter(key_event_forwarder_.get());
        }
    }
}

void QApplicationStartup::failStartup(const QString& message)
{
    if (startup_finished_)
        return;
    startup_finished_ = true;
    spdlog::error("startup: {}", message.toStdString());
    app_.exit(-1);
}

#ifndef __EMSCRIPTEN__
bool QApplicationStartup::connectStartupReadiness()
{
    main_window_ = qobject_cast<QQuickWindow*>(engine_->rootObjects().first());
    if (!main_window_) {
        failStartup(QCoreApplication::translate("Startup", "未创建主窗口"));
        return false;
    }
    auto* viewport = findViewport(main_window_->contentItem());
    if (!viewport) {
        failStartup(QCoreApplication::translate("Startup", "三维视口加载失败"));
        return false;
    }
    startup_error_connection_ = connect(main_window_, &QQuickWindow::sceneGraphError, this, [this](QQuickWindow::SceneGraphError, const QString& message) { failStartup(message); }, Qt::QueuedConnection);

    // 队列在视口初始化之后消费；已初始化的视口也会在后续同步中执行。
    viewport->dispatch_async([this](vtkRenderWindow*, QQuickVTKItem::vtkUserData) {
        QMetaObject::invokeMethod(this, &QApplicationStartup::waitForPresentedFrame, Qt::QueuedConnection);
    });
    return true;
}

void QApplicationStartup::waitForPresentedFrame()
{
    if (startup_finished_ || !main_window_)
        return;

    // VTK 就绪后在 GUI 线程安装一次性呈现通知，再请求主窗口更新。
    const auto queued_once = Qt::ConnectionType(Qt::QueuedConnection | Qt::SingleShotConnection);
    connect(main_window_, &QQuickWindow::frameSwapped, this, &QApplicationStartup::finishStartup, queued_once);
    main_window_->update();
}

void QApplicationStartup::finishStartup()
{
    if (startup_finished_)
        return;
    startup_finished_ = true;
    // 恢复 Qt 对运行期间场景图错误的默认处理。
    disconnect(startup_error_connection_);
    spdlog::info("startup: main window ready after viewport and presented frame");
    // 先停止动画，再释放背景、绘制缓冲和封面专属连接。
    splash_->hide();
    splash_.reset();
    app_.setQuitOnLastWindowClosed(true);
    if (main_window_)
        main_window_->requestActivate();
    installKeyEventForwarder();
}
#endif
