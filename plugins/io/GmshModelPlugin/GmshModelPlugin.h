/**
 * @file GmshModelPlugin.h
 * @brief Gmsh 网格格式读写插件
 */
#ifndef GMSH_MODEL_PLUGIN_H
#define GMSH_MODEL_PLUGIN_H
#include "GmshModelHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include <QObject>

namespace systems::io {
class GmshModelPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.io.GmshModelPlugin/1.0" FILE "GmshModelPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<GmshModelHandler, ModelIOHandler>::get();
    }
};
}
#endif // !GMSH_MODEL_PLUGIN_H
