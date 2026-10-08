#pragma once
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include "SplitEdgeHandler.h"
#include <QObject>
namespace systems::feature {
/** @brief 分割几何边插件入口。 */
class SplitEdgePlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.SplitEdgePlugin/1.0" FILE "SplitEdgePlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    { return HandlerCreatorDestroyerFactory<SplitEdgeHandler, FeatureHandler>::get(); }
};
}
