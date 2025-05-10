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

int main(int argc, char* argv[])
{
    QQuickVTKItem::setGraphicsApi();
    QModelObserver observer;
    ModelManager manager(nullptr, &observer);
    QModelQuery query(&manager, nullptr);

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({ { "modelManager", QVariant::fromValue(&manager) },
        { "modelObserver", QVariant::fromValue(&observer) },
        { "modelQuery", QVariant::fromValue(&query) } });
    engine.loadFromModule("fileLoader", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;


    return app.exec();
}
