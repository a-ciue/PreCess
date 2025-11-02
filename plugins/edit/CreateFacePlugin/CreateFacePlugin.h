#pragma once
#include "PluginBase.h"
#include "CreateFaceHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include <QObject>

namespace systems::edit {
class CreateFacePlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.edit.CreateFacePlugin/1.0" FILE "CreateFacePlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<CreateFaceHandler, EditHandler>::get();
    }
};
}