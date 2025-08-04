/**
 * @file OBJModelPlugin.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "../HandlerCreatorDestroyerFactory.h"
#include "../PluginBase.h"
#include "OBJModelHandler.h"
#include <QObject>

namespace systems::io {
class OBJModelPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.io.OBJModelPlugin/1.0" FILE "OBJModelPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<OBJModelHandler, ModelIOHandler>::get();
    }
};
}