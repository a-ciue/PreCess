/**
 * @file QmlRegistration.cpp
 * @brief QML 类型注册与组件初始化
 */

#include "QmlRegistration.h"

#include "DockPanelItem.h"
#include "DockHostItem.h"
#include "GlobalMouseRelay.h"
#include "DockRuntime.h"
#include "docking/DockEnums.h"

#include <QCoreApplication>
#include <QQmlEngine>
#include <QtQml/qqml.h>

// 资源初始化须在全局命名空间进行（Q_INIT_RESOURCE 声明的符号无命名空间）
static void initDockQmlResources()
{
    Q_INIT_RESOURCE(dock_qml);
}

namespace dock::ui {

void registerTypes()
{
    qmlRegisterType<DockHostItem>("PreCess.Docking", 1, 0, "DockHost");
    qmlRegisterType<DockPanelItem>("PreCess.Docking", 1, 0, "DockPanel");
    qmlRegisterUncreatableMetaObject(dock::staticMetaObject, "PreCess.Docking", 1, 0, "Tokens",
        QStringLiteral("Enum access only"));
}

}

namespace dock {

void init(QQmlEngine* engine)
{
    initDockQmlResources();

    ui::registerTypes();
    ui::DockRuntime::instance().setEngine(engine);

    if (QCoreApplication::instance()
        && !QCoreApplication::instance()->property("dockMouseRelayInstalled").toBool()) {
        QCoreApplication::instance()->installEventFilter(new ui::GlobalMouseRelay(
            QCoreApplication::instance()));
        QCoreApplication::instance()->setProperty("dockMouseRelayInstalled", true);
    }
}

}
