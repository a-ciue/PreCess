#include "QApplicationStartup.h"
#include "QModelManager.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQuickStyle>
#ifndef __EMSCRIPTEN__
#include <QQuickVTKItem.h>
#endif
#include <spdlog/cfg/env.h>

#ifdef __EMSCRIPTEN__
// 浏览器事件循环在 main 返回后运行，应用及启动对象须继续存活。
#define WASM_GLOBAL static
#else
#define WASM_GLOBAL
#endif

int main(int argc, char* argv[])
{
    spdlog::cfg::load_env_levels();
    // WebGL/OpenGLES 不支持 OpenGL 3.2
#ifndef __EMSCRIPTEN__
    QQuickVTKItem::setGraphicsApi();
#endif
    QModelManager::argv0 = argv[0];

    WASM_GLOBAL QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/images/ZenithGrid.ico"));
    // 持久化身份（QSettings 存储位置）：布局快照等依赖明确的应用名/组织名
    QCoreApplication::setOrganizationName(QStringLiteral("PreCess"));
    QCoreApplication::setApplicationName(QStringLiteral("PreCess"));
    QCoreApplication::setApplicationVersion(QStringLiteral(PRECESS_APP_VERSION));
    // 全平台统一 Fusion 运行时样式：QML 一律用运行时样式选择（裸 import
    // QtQuick.Controls），禁止文件内显式 import 特定样式，避免样式混用。
    // 控件调色板由 Main.qml 的 ApplicationWindow 绑定 Theme 令牌；Fusion 的
    // 默认控件调色板取自平台主题，应用调色板只覆盖 ToolTip 等弹窗。
    QQuickStyle::setStyle("Fusion");

    WASM_GLOBAL QApplicationStartup startup(app);
    return startup.run();
}
