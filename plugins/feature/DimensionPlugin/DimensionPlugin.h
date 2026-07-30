/**
 * @file DimensionPlugin.h
 * @brief 尺寸标注插件声明：将 DimensionHandler 注册到功能系统
 * @author 范成通 email 1941804585@qq.com
 */
#pragma once
#include "DimensionHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"

#include <QObject>

namespace systems::feature {
class DimensionPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.DimensionPlugin/1.0" FILE "DimensionPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<DimensionHandler, FeatureHandler>::get();
    }
};
}
