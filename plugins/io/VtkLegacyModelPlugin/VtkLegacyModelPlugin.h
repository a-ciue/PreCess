/**
 * @file VtkLegacyModelPlugin.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef VTK_LEGACY_MODEL_PLUGIN_H
#define VTK_LEGACY_MODEL_PLUGIN_H
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include "VtkLegacyModelHandler.h"
#include <QObject>

namespace systems::io {
class VtkLegacyModelPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.io.VtkLegacyModelPlugin/1.0" FILE "VtkLegacyModelPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<VtkLegacyModelHandler, ModelIOHandler>::get();
    }
};
}
#endif // !VTK_LEGACY_MODEL_PLUGIN_H