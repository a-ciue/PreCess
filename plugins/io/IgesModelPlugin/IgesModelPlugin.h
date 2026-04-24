/**
 * @file IgesModelPlugin.h
 * @brief IGES 文件格式读写插件
 * @author 范成通
 */
#ifndef IGES_MODEL_PLUGIN_H
#define IGES_MODEL_PLUGIN_H
#include "HandlerCreatorDestroyerFactory.h"
#include "IgesModelHandler.h"
#include "PluginBase.h"
#include <QObject>

namespace systems::io {
class IgesModelPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.io.IgesModelPlugin/1.0" FILE "IgesModelPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<IgesModelHandler, ModelIOHandler>::get();
    }
};
}
#endif // !IGES_MODEL_PLUGIN_H