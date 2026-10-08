#pragma once
#include "AutoGeometryRepairHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"

#include <QObject>

namespace systems::feature {
/**
 * @brief 将自动几何间隙修复功能注册到 FeatureSystem 的插件入口。
 */
class AutoGeometryRepairPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.AutoGeometryRepairPlugin/1.0" FILE "AutoGeometryRepairPlugin.json")

private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<AutoGeometryRepairHandler, FeatureHandler>::get();
    }
};
}
