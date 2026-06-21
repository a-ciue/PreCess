#pragma once
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include "TetGenLibHandler.h"

#include <QObject>

namespace systems::algo {
class TetGenLibPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.algo.TetGenLibPlugin/1.0" FILE "TetGenLibPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<TetGenLibHandler, AlgorithmHandler>::get();
    }
};
}
