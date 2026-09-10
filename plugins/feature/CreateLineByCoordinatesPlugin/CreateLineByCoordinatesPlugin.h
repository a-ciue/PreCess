#pragma once
#include "CreateLineByCoordinatesHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include <QObject>

namespace systems::feature {
class CreateLineByCoordinatesPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.CreateLineByCoordinatesPlugin/1.0" FILE "CreateLineByCoordinatesPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<CreateLineByCoordinatesHandler, FeatureHandler>::get();
    }
};
}
