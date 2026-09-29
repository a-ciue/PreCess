#pragma once
#include "HandlerCreatorDestroyerFactory.h"
#include "MergeEdgeHandler.h"
#include "PluginBase.h"

#include <QObject>

namespace systems::feature {
/**
 * @brief 将合并几何边功能注册到 FeatureSystem 的插件入口。
 */
class MergeEdgePlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.MergeEdgePlugin/1.0" FILE "MergeEdgePlugin.json")

private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<MergeEdgeHandler, FeatureHandler>::get();
    }
};
}
