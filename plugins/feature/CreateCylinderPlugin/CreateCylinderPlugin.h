#pragma once
#include "CreateCylinderHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include <QObject>

namespace systems::feature {
class CreateCylinderPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.CreateCylinderPlugin/1.0" FILE "CreateCylinderPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<CreateCylinderHandler, FeatureHandler>::get();
    }
};
}
