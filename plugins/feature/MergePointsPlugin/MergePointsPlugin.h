#pragma once
#include "HandlerCreatorDestroyerFactory.h"
#include "MergePointsHandler.h"
#include "PluginBase.h"

#include <QObject>

namespace systems::feature {
/**
 * @brief 将点合并功能注册到 FeatureSystem 的插件入口。
 */
class MergePointsPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.MergePointsPlugin/1.0" FILE "MergePointsPlugin.json")

private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<MergePointsHandler, FeatureHandler>::get();
    }
};
}
