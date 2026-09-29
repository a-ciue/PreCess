#pragma once
#include "HandlerCreatorDestroyerFactory.h"
#include "MergeFaceHandler.h"
#include "PluginBase.h"

#include <QObject>

namespace systems::feature {
/**
 * @brief 将合并几何面功能注册到 FeatureSystem 的插件入口。
 */
class MergeFacePlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.MergeFacePlugin/1.0" FILE "MergeFacePlugin.json")

private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<MergeFaceHandler, FeatureHandler>::get();
    }
};
}
