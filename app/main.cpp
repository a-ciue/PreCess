#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>

#include <QtQuick/QQuickWindow>

#include <QtGui/QGuiApplication>

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QQuickVTKItem.h>

#include "QModelObserver.h"
#include "QModelQuery.h"
#include <QtQml/QQmlExtensionPlugin>

#include "QModelManager.h"
#include "QAlgorithmSystemAdaptor.h"

#include <spdlog/spdlog.h>
#include <spdlog/cfg/env.h>

class ModelLayer;

int main(int argc, char* argv[])
{
    spdlog::cfg::load_env_levels();
    QQuickVTKItem::setGraphicsApi();

    QModelManager q_manager(argv[0]);
    ModelLayer* manager = q_manager.getModelManager();
    QModelObserver* observer = q_manager.getModelObserver();
    systems::algo::QAlgorithmSystemAdaptor algoAdaptor = q_manager.getAlgorithmSystemAdaptor();
    systems::io::QModelIOSystemAdaptor ioAdaptor = q_manager.getModelIOSystemAdaptor();
    systems::edit::QEditSystemAdaptor editAdaptor = q_manager.getEditSystemAdaptor();
    QModelQuery query(manager, nullptr);

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/images/PreCess.ico"));

    QQmlApplicationEngine engine;

    // 收集命令行参数（跳过第一个参数，它是程序路径）
    QStringList arguments_str = app.arguments().mid(1);
    QList<QUrl> arguments;
    for (auto& argument_str : arguments_str) {
        arguments << QUrl::fromLocalFile(argument_str);
    }

    // 将参数列表暴露给QML
    engine.rootContext()->setContextProperty("commandLineArgs", QVariant::fromValue(arguments));

    engine.setInitialProperties({
        { "modelManager", QVariant::fromValue(&q_manager) },
        { "modelObserver", QVariant::fromValue(observer) },
        { "modelQuery", QVariant::fromValue(&query) },
        { "algorithmSystem", QVariant::fromValue(&algoAdaptor) },
        { "ioSystem", QVariant::fromValue(&ioAdaptor) } ,
        { "editSystem", QVariant::fromValue(&editAdaptor) } });
    engine.loadFromModule("app", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
