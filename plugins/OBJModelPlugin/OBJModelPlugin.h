/**
 * @file OBJModelPlugin.h
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "../PluginBase.h"
#include "OBJModelHandler.h"
#include <QObject>
#include <memory>

class OBJModelPlugin : public QObject, public systems::PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.io.OBJModelPlugin/1.0" FILE "OBJModelPlugin.json")
public:
    std::any makeHandler() override
    {
        std::shared_ptr<systems::io::ModelIOHandler> handler = std::make_shared<systems::io::OBJModelHandler>();
        return handler;
    }
};
