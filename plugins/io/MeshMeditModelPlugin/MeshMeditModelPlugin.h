/**
 * @file MeshMeditModelPlugin.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef MESH_MEDIT_MODEL_PLUGIN_H
#define MESH_MEDIT_MODEL_PLUGIN_H
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include "MeshMeditModelHandler.h"
#include <QObject>

namespace systems::io {
class MeshMeditModelPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.io.MeshMeditModelPlugin/1.0" FILE "MeshMeditModelPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<MeshMeditModelHandler, ModelIOHandler>::get();
    }
};
}
#endif // !MESH_MEDIT_MODEL_PLUGIN_H