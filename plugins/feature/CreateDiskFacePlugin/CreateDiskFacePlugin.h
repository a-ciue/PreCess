#pragma once
#include "CreateDiskFaceHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include <QObject>

namespace systems::feature {
class CreateDiskFacePlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.CreateDiskFacePlugin/1.0" FILE "CreateDiskFacePlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<CreateDiskFaceHandler, FeatureHandler>::get();
    }
};
}
