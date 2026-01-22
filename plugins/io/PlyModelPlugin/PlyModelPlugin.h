/**
 * @file PlyModelPlugin.h
 * @author 龚正(1740124400@qq.com)
 */
#ifndef PLY_MODEL_PLUGIN_H
#define PLY_MODEL_PLUGIN_H
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include "PlyModelHandler.h"
#include <QObject>

namespace systems::io {
class PlyModelPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.io.PlyModelPlugin/1.0" FILE "PlyModelPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<PlyModelHandler, ModelIOHandler>::get();
    }
};
}
#endif // !PLY_MODEL_PLUGIN_H