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

class ModelManager;

int main(int argc, char* argv[])
{
    QQuickVTKItem::setGraphicsApi();
    QModelManager q_manager;
    ModelManager* manager = q_manager.getModelManager();
    QModelObserver* observer = q_manager.getModelObserver();
    systems::algo::QAlgorithmSystemAdaptor algoAdaptor = q_manager.getAlgorithmSystemAdaptor();
    systems::io::QModelIOSystemAdaptor ioAdaptor = q_manager.getModelIOSystemAdaptor();
    QModelQuery query(manager, nullptr);

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/images/PreCess.ico"));

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        { "modelManager", QVariant::fromValue(&q_manager) },
        { "modelObserver", QVariant::fromValue(observer) },
        { "modelQuery", QVariant::fromValue(&query) },
        { "algorithmSystem", QVariant::fromValue(&algoAdaptor) },
        { "ioSystem", QVariant::fromValue(&ioAdaptor) } });
    engine.loadFromModule("app", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
