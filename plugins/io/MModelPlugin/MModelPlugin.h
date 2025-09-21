/**
 * @file MModelPlugin.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#ifndef M_MODEL_PLUGIN_H
#define M_MODEL_PLUGIN_H
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include "MModelHandler.h"
#include <QObject>

namespace systems::io {
class MModelPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.io.MModelPlugin/1.0" FILE "MModelPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<MModelHandler, ModelIOHandler>::get();
    }
};
}
#endif // !M_MODEL_PLUGIN_H