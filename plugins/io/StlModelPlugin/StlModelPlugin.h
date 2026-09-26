/**
 * @file StlModelPlugin.h
 * @brief STL 三角网格格式读写插件
 */
#ifndef STL_MODEL_PLUGIN_H
#define STL_MODEL_PLUGIN_H
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include "StlModelHandler.h"
#include <QObject>

namespace systems::io {
class StlModelPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.io.StlModelPlugin/1.0" FILE "StlModelPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<StlModelHandler, ModelIOHandler>::get();
    }
};
}
#endif // !STL_MODEL_PLUGIN_H
