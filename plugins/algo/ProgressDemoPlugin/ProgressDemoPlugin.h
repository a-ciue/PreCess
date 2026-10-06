#pragma once
#include "PluginBase.h"
#include "ProgressDemoHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include <QObject>

namespace systems::algo {
class ProgressDemoPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.algo.ProgressDemoPlugin/1.0" FILE "ProgressDemoPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<ProgressDemoHandler, AlgorithmHandler>::get();
    }
};
}
