/**
 * @file InpModelPlugin.h
 * @author 龚正(1740124400@qq.com)
 */
#ifndef INP_MODEL_PLUGIN_H
#define INP_MODEL_PLUGIN_H
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include "InpModelHandler.h"
#include <QObject>

namespace systems::io {
class InpModelPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.io.InpModelPlugin/1.0" FILE "InpModelPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<InpModelHandler, ModelIOHandler>::get();
    }
};
}
#endif // !INP_MODEL_PLUGIN_H