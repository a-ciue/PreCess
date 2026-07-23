#pragma once

#include "HandlerCreatorDestroyerFactory.h"
#include "MeshQualityHandler.h"
#include "PluginBase.h"

#include <QObject>

namespace systems::feature {

/**
 * @brief 网格质量 Feature 插件入口
 */
class MeshQualityPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.MeshQualityPlugin/1.0" FILE "MeshQualityPlugin.json")

private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<MeshQualityHandler, FeatureHandler>::get();
    }
};

}
