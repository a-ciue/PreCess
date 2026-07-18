#pragma once
#include "PluginBase.h"
#include "FeatureDemoHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include <QObject>

namespace systems::feature {
class FeatureDemoPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.FeatureDemoPlugin/1.0" FILE "FeatureDemoPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<FeatureDemoHandler, FeatureHandler>::get();
    }
};
}
