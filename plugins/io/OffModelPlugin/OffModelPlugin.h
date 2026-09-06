/**
 * @file OffModelPlugin.h
 * @brief OFF 文件格式读写插件
 */
#ifndef OFF_MODEL_PLUGIN_H
#define OFF_MODEL_PLUGIN_H
#include "HandlerCreatorDestroyerFactory.h"
#include "OffModelHandler.h"
#include "PluginBase.h"
#include <QObject>

namespace systems::io {
class OffModelPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.io.OffModelPlugin/1.0" FILE "OffModelPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<OffModelHandler, ModelIOHandler>::get();
    }
};
}
#endif // !OFF_MODEL_PLUGIN_H
