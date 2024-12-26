#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>

#include <QtQuick/QQuickWindow>

#include <QtGui/QGuiApplication>

#include <QtCore/QPointer>
#include <QtCore/QScopedPointer>
#include <QQuickVTKItem.h>

#include "ModelManager.h"

int main(int argc, char* argv[])
{
    QQuickVTKItem::setGraphicsApi();
    ModelManager manager;

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({ { "modelManager", QVariant::fromValue(&manager) } });
    //engine.load(QUrl(QStringLiteral("qrc:/qt/qml/fileLoader/main.qml")));
    engine.loadFromModule("fileLoader", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;


    return app.exec();
}
