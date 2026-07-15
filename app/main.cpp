#include "QLogManager.h"
#include "QModelManager.h"
#include <QQuickVTKItem.h>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <kddockwidgets/qtquick/Platform.h>
#include <spdlog/cfg/env.h>

int main(int argc, char* argv[])
{
    spdlog::cfg::load_env_levels();
    QQuickVTKItem::setGraphicsApi();
    QModelManager::argv0 = argv[0];

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/images/PreCess.ico"));
    KDDockWidgets::initFrontend(KDDockWidgets::FrontendType::QtQuick);

    QLogManager::initialize();

    QQmlApplicationEngine engine;
    KDDockWidgets::QtQuick::Platform::instance()->setQmlEngine(&engine);

    // 收集命令行参数（跳过第一个参数，它是程序路径）
    QStringList arguments_str = app.arguments().mid(1);
    QList<QUrl> arguments;
    for (auto& argument_str : arguments_str) {
        arguments << QUrl::fromLocalFile(argument_str);
    }

    engine.rootContext()->setContextProperty("QLogManager", QLogManager::instance());

    // 将参数列表暴露给QML
    engine.rootContext()->setContextProperty("commandLineArgs", QVariant::fromValue(arguments));

    engine.loadFromModule("app", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
