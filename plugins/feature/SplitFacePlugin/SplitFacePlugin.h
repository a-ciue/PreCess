#pragma once
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include "SplitFaceHandler.h"

#include <QObject>

namespace systems::feature {
/**
 * @brief 将分割几何面功能注册到 FeatureSystem 的插件入口。
 */
class SplitFacePlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.SplitFacePlugin/1.0" FILE "SplitFacePlugin.json")

private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<SplitFaceHandler, FeatureHandler>::get();
    }
};
}
