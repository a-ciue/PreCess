#pragma once
#include "../../systems/PluginBase.h"
#include <QObject>
#include <memory>
#include <vector>
#include <any>
#include <string>
#include <filesystem>
#include "CmdExecuteHandler.h"

class CmdExecutePlugin : public QObject, public systems::PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.algo.CmdExecutePlugin/1.0" FILE "CmdExecutePlugin.json")
public:
    std::any makeHandler() override {
        using namespace systems::algo;
        std::shared_ptr<AlgorithmHandler> handler = std::make_shared<CmdExecuteHandler>();
        return handler;
    }
};
