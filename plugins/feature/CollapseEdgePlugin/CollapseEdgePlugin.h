#pragma once
#include "CollapseEdgeHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include <QObject>
namespace systems::feature {
/** @brief 压缩几何边插件入口。 */
class CollapseEdgePlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.CollapseEdgePlugin/1.0" FILE "CollapseEdgePlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    { return HandlerCreatorDestroyerFactory<CollapseEdgeHandler, FeatureHandler>::get(); }
};
}
