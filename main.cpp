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
#include "commands/SplitEdgeCommand.h"
#include "commands/MergeBlocksCommand.h"
#include "commands/TrianglulationCommand.h"
#include <QtQml/QQmlExtensionPlugin>

#include "QModelManager.h"

Q_IMPORT_QML_PLUGIN(modelPlugin)

int main(int argc, char* argv[])
{
    QQuickVTKItem::setGraphicsApi();
    QModelManager q_manager;
    ModelManager* manager = q_manager.getModelManager();
    QModelObserver* observer = q_manager.getModelObserver();
    QModelQuery query(manager, nullptr);

    QCommandCatalog catalog;
    catalog.addCommand(new QCommand("切分面", "faceMode.splitFace", SplitFaceCommand::create, SplitFaceCommand::getArgsModel()));
    catalog.addCommand(new QCommand("切分边", "faceMode.splitEdge", SplitEdgeCommand::create, SplitEdgeCommand::getArgsModel()));
    catalog.addCommand(new QCommand("合并块", "blockMode.mergeBlocks", MergeBlocksCommand::create, MergeBlocksCommand::getArgsModel()));
    catalog.addCommand(new QCommand("网格剖分", "algorithm.triangulation", TrianglulationCommand::create, TrianglulationCommand::getArgsModel()));
    CommandDispatcher dispatcher(manager);

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        { "modelManager", QVariant::fromValue(&q_manager) },
        { "modelObserver", QVariant::fromValue(observer) },
        { "modelQuery", QVariant::fromValue(&query) },
        { "commandCatalog", QVariant::fromValue(&catalog) },
        { "commandDispatcher", QVariant::fromValue(&dispatcher) },
    });
    engine.loadFromModule("fileLoader", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;


    return app.exec();
}
