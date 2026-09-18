/**
 * @file QmlTypes.cpp
 * @brief QML 类型注册与组件初始化
 */

#include "QmlTypes.h"

#include "DockWidgetInstantiator.h"
#include "DockingAreaView.h"
#include "MouseGrabber.h"
#include "Platform.h"
#include "core/DockTypes.h"

#include <QCoreApplication>
#include <QQmlEngine>
#include <QtQml/qqml.h>

// 资源初始化须在全局命名空间进行（Q_INIT_RESOURCE 声明的符号无命名空间）
static void initDockQmlResources()
{
    Q_INIT_RESOURCE(dock_qml);
}

namespace dock::qtquick {

void registerTypes()
{
    qmlRegisterType<DockingAreaView>("PreCess.Docking", 1, 0, "DockingArea");
    qmlRegisterType<DockWidgetInstantiator>("PreCess.Docking", 1, 0, "DockWidget");
    qmlRegisterUncreatableMetaObject(dock::staticMetaObject, "PreCess.Docking", 1, 0, "Enums",
        QStringLiteral("Enum access only"));
}

}

namespace dock {

void init(QQmlEngine* engine)
{
    initDockQmlResources();

    qtquick::registerTypes();
    qtquick::Platform::instance().setEngine(engine);

    if (QCoreApplication::instance()
        && !QCoreApplication::instance()->property("dockMouseGrabberInstalled").toBool()) {
        QCoreApplication::instance()->installEventFilter(new qtquick::MouseGrabber(
            QCoreApplication::instance()));
        QCoreApplication::instance()->setProperty("dockMouseGrabberInstalled", true);
    }
}

}
