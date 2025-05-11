#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>

#include <QtQuick/QQuickWindow>

#include <QtGui/QGuiApplication>

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QQuickVTKItem.h>

#include "ModelManager.h"
#include "ModelObserver.h"
#include "ModelQuery.h"
#include "commands/QCommandCatalog.h"
#include "commands/CommandDispatcher.h"
#include "commands/SplitFaceCommand.h"

int main(int argc, char* argv[])
{
    QQuickVTKItem::setGraphicsApi();
    QModelObserver observer;
    ModelManager manager(nullptr, &observer);
    QModelQuery query(&manager, nullptr);

    QCommandCatalog catalog;
    catalog.addCommand(new QCommand("切分面", SplitFaceCommand::create, SplitFaceCommand::getArgsModel()));
    CommandDispatcher dispatcher(&manager);

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        { "modelManager", QVariant::fromValue(&manager) },
        { "modelObserver", QVariant::fromValue(&observer) },
        { "modelQuery", QVariant::fromValue(&query) },
        { "commandCatalog", QVariant::fromValue(&catalog) },
        { "commandDispatcher", QVariant::fromValue(&dispatcher) },
    });
    engine.loadFromModule("fileLoader", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;


    return app.exec();
}
