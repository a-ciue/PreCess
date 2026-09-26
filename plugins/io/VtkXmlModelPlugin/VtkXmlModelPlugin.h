/**
 * @file VtkXmlModelPlugin.h
 * @brief VTK XML 网格格式（.vtp/.vtu）读写插件
 */
#ifndef VTK_XML_MODEL_PLUGIN_H
#define VTK_XML_MODEL_PLUGIN_H
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"
#include "VtkXmlModelHandler.h"
#include <QObject>

namespace systems::io {
class VtkXmlModelPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.io.VtkXmlModelPlugin/1.0" FILE "VtkXmlModelPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<VtkXmlModelHandler, ModelIOHandler>::get();
    }
};
}
#endif // !VTK_XML_MODEL_PLUGIN_H
