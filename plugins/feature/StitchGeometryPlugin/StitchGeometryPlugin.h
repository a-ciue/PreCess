#pragma once
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include "StitchGeometryHandler.h"

#include <QObject>

namespace systems::feature {
/**
 * @brief 将几何缝合功能注册到 FeatureSystem 的插件入口。
 */
class StitchGeometryPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.StitchGeometryPlugin/1.0" FILE "StitchGeometryPlugin.json")

private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<StitchGeometryHandler, FeatureHandler>::get();
    }
};
}
