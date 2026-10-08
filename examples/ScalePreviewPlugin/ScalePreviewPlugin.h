#pragma once
#include "PluginBase.h"
#include "ScalePreviewHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include <QObject>

namespace systems::feature {
class ScalePreviewPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.ScalePreviewPlugin/1.0" FILE "ScalePreviewPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<ScalePreviewHandler, FeatureHandler>::get();
    }
};
}
