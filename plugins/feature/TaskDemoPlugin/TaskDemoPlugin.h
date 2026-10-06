#pragma once
#include "PluginBase.h"
#include "TaskDemoHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include <QObject>

namespace systems::feature {
class TaskDemoPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.TaskDemoPlugin/1.0" FILE "TaskDemoPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<TaskDemoHandler, FeatureHandler>::get();
    }
};
}
