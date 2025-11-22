#pragma once
#include "PluginBase.h"
#include "DeleteFaceHandler.h"
#include "HandlerCreatorDestroyerFactory.h"
#include <QObject>

namespace systems::edit {
class DeleteFacePlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.edit.DeleteFacePlugin/1.0" FILE "DeleteFacePlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override final
    {
        return HandlerCreatorDestroyerFactory<DeleteFaceHandler, EditHandler>::get();
    }
};
}