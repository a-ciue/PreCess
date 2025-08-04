#pragma once
#include "../PluginBase.h"
#include "CmdExecuteHandler.h"
#include "../HandlerCreatorDestroyerFactory.h"
#include <QObject>

namespace systems::algo {
class CmdExecutePlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.algo.CmdExecutePlugin/1.0" FILE "CmdExecutePlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<CmdExecuteHandler, AlgorithmHandler>::get();
    }
};
}