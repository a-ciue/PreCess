#pragma once
#include "PatchFaceHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"

#include <QObject>

namespace systems::feature {
/**
 * @brief 将补面功能注册到 FeatureSystem 的插件入口。
 */
class PatchFacePlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.PatchFacePlugin/1.0" FILE "PatchFacePlugin.json")

private:
    //! @brief 提供补面处理器的创建与销毁工厂。
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<PatchFaceHandler, FeatureHandler>::get();
    }
};
}
