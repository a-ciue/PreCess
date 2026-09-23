/**
 * @file QhexModelPlugin.h
 * @brief Qhex 实验室六面体网格格式读写插件
 */
#ifndef QHEX_MODEL_PLUGIN_H
#define QHEX_MODEL_PLUGIN_H
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include "QhexModelHandler.h"
#include <QObject>

namespace systems::io {
class QhexModelPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.io.QhexModelPlugin/1.0" FILE "QhexModelPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<QhexModelHandler, ModelIOHandler>::get();
    }
};
}
#endif // !QHEX_MODEL_PLUGIN_H
