#pragma once
#include "PluginBase.h"
#include "GmshMeshHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include <QObject>

namespace systems::algo {
class GmshMeshPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.algo.GmshMeshPlugin/1.0"
                      FILE "GmshPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<GmshMeshHandler, AlgorithmHandler>::get();
    }
};
} // namespace systems::algo