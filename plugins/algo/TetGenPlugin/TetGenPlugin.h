#pragma once
#include "PluginBase.h"
#include "TetGenHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include <QObject>

namespace systems::algo {
class TetGenPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.algo.TetGenPlugin/1.0" FILE "TetGenPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<TetGenHandler, AlgorithmHandler>::get();
    }
};
}