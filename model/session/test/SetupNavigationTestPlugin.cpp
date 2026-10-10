/**
 * @file SetupNavigationTestPlugin.cpp
 * @brief 验证 Feature 导航声明经真实 DLL 边界注册，不安装到产品插件目录。
 */
#include "FeatureHandler.h"
#include "FeatureRegistrar.h"
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"

#include <QObject>

namespace systems::feature {
class SetupNavigationTestHandler : public FeatureHandler {
public:
    void setup(FeatureRegistrar& registrar, FeatureContext&) override
    {
        registrar.addParameter({ ArgTypeEnum::Float, "Size", "1", "" });
        auto& navigation = registrar.navigation();
        navigation.addEntry({ "setup-custom", "Setup custom category", "", 5, "Tools/Custom" });
        navigation.setLabel("Setup feature");
        navigation.setOrder(7);
        navigation.setEntryDefault("setup-custom", "Size", "3");
    }
};

class SetupNavigationTestPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.SetupNavigationTestPlugin/1.0" FILE "SetupNavigationTestPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override
    {
        return HandlerCreatorDestroyerFactory<SetupNavigationTestHandler, FeatureHandler>::get();
    }
};
}

#include "SetupNavigationTestPlugin.moc"
