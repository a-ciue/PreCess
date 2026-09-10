#pragma once
#include "CreateFaceFromEdgesHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include <QObject>

namespace systems::feature {
class CreateFaceFromEdgesPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.CreateFaceFromEdgesPlugin/1.0" FILE "CreateFaceFromEdgesPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<CreateFaceFromEdgesHandler, FeatureHandler>::get();
    }
};
}
