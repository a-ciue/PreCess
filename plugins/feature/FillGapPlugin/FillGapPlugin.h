#pragma once
#include "FillGapHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"

#include <QObject>

namespace systems::feature {
/**
 * @brief 将局部缝合功能注册到 FeatureSystem 的插件入口。
 */
class FillGapPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.FillGapPlugin/1.0" FILE "FillGapPlugin.json")

private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<FillGapHandler, FeatureHandler>::get();
    }
};
}
