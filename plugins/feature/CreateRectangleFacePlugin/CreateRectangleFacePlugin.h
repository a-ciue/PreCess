#pragma once
#include "CreateRectangleFaceHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include <QObject>

namespace systems::feature {
class CreateRectangleFacePlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.CreateRectangleFacePlugin/1.0" FILE "CreateRectangleFacePlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<CreateRectangleFaceHandler, FeatureHandler>::get();
    }
};
}
