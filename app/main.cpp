#include "QLogManager.h"
#include "QModelManager.h"
#include "QFeatureSystemAdaptor.h"
#include <QKeyEvent>
#include <QQuickVTKItem.h>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <kddockwidgets/qtquick/Platform.h>
#include <spdlog/cfg/env.h>
#ifdef __EMSCRIPTEN__
#include <QtGui/QFontDatabase>
#include <QtQuick/QQuickWindow>
#include <QtQuick/QSGRendererInterface>
#include <QtQuickControls2/QQuickStyle>
#include "QWasmBridge.h"
#endif

#ifdef __EMSCRIPTEN__
// wasm 下将 Qt 相关对象提升为全局变量，避免在 main 函数返回后被销毁
#define WASM_GLOBAL static
#else
#define WASM_GLOBAL
#endif

namespace {
/**
 * @brief 全局按键事件转发器：把按键事件投递到功能系统
 *
 * 事件被功能消费（如按键绑定命中）时立即 accept，不再向焦点控件传递，
 * 避免同一按键被重复消费。
 */
class KeyEventForwarder : public QObject {
public:
    KeyEventForwarder(systems::feature::QFeatureSystemAdaptor& adaptor, QObject* parent = nullptr)
        : QObject(parent)
        , adaptor_(&adaptor)
    {
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
            const auto* key_event = static_cast<QKeyEvent*>(event);
            if (!key_event->isAutoRepeat()
                && adaptor_->postKeyEvent(key_event->key(), static_cast<int>(key_event->modifiers()),
                    event->type() == QEvent::KeyPress)) {
                return true; // 功能已消费该按键事件，阻止继续传递
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    systems::feature::QFeatureSystemAdaptor* adaptor_;
};
}

int main(int argc, char* argv[])
{
    spdlog::cfg::load_env_levels();
    // WebGL/OpenGLES 不支持 OpenGL 3.2
#ifndef __EMSCRIPTEN__
    QQuickVTKItem::setGraphicsApi();
#endif
    QModelManager::argv0 = argv[0];

    WASM_GLOBAL QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/images/PreCess.ico"));
#ifdef __EMSCRIPTEN__
    // wasm 平台需手动加载捆绑的字体，否则无法显示中文
    int font_id = QFontDatabase::addApplicationFont(":/fonts/appfont.bin");
    QStringList font_families = QFontDatabase::applicationFontFamilies(font_id);
    if (!font_families.isEmpty()) {
        app.setFont(QFont(font_families.first()));
    }
    QQuickStyle::setStyle("Fusion");
#endif
    KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtQuick);

    QLogManager::initialize();

    WASM_GLOBAL QQmlApplicationEngine engine;
    KDDockWidgets::QtQuick::Platform::instance()->setQmlEngine(&engine);

    // 收集命令行参数（跳过第一个参数，它是程序路径）
    QStringList arguments_str = app.arguments().mid(1);
    QList<QUrl> arguments;
    for (auto& argument_str : arguments_str) {
        arguments << QUrl::fromLocalFile(argument_str);
    }

    engine.rootContext()->setContextProperty("QLogManager", QLogManager::instance());
#ifdef __EMSCRIPTEN__
    engine.rootContext()->setContextProperty("QWasmBridge", new QWasmBridge(&app));
#endif
    // 将参数列表暴露给QML
    engine.rootContext()->setContextProperty("commandLineArgs", QVariant::fromValue(arguments));

    engine.loadFromModule("app", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;

    // 安装全局按键事件过滤器，将按键事件转发给功能系统
    if (auto* model_manager = engine.singletonInstance<QModelManager*>("app.model", "QModelManager")) {
        if (auto* feature_adaptor = model_manager->getFeatureSystemAdaptor()) {
            app.installEventFilter(new KeyEventForwarder(*feature_adaptor, &app));
        }
    }

#ifdef __EMSCRIPTEN__
    // wasm 平台由浏览器管理事件循环，main 函数直接返回即可
    return 0;
#else
    return app.exec();
#endif
}
