/**
 * @file SetupNavigationTestPlugin.cpp
 * @brief 验证 Feature 导航声明经真实 DLL 边界注册，不安装到产品插件目录。
 */
#include "FeatureHandler.h"
#include "FeatureRegistrar.h"
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"

#include <QObject>
#include <stdexcept>

namespace systems::feature {
class SetupNavigationTestHandler : public FeatureHandler {
public:
    void teardown(FeatureContext&) override
    {
        const auto failure = qgetenv("PRECESS_TEST_SETUP_TEARDOWN");
        if (failure == "standard")
            throw std::runtime_error("expected plugin teardown failure");
        if (failure == "unknown")
            throw 42;
    }
    void setup(FeatureRegistrar& registrar, FeatureContext&) override
    {
        registrar.addParameter({ ArgTypeEnum::Float, "Size", "1", "" });
        registrar.addNavigationMenu({ "setup-tools", "Tools" });
        registrar.addNavigationGroup({ "setup-tools", "custom", "Custom" });
        auto& navigation = registrar.navigation();
        navigation.addEntry({ "setup-custom", "Setup custom category", "", 5, "setup-tools", "custom" });
        navigation.setLabel("Setup feature");
        navigation.setOrder(7);
        navigation.setEntryDefault("setup-custom", "Size", "3");
    }
};

class SetupNavigationTestPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.feature.SetupNavigationTestPlugin/1.0" FILE "SetupNavigationTestPlugin.json")
public:
    SetupNavigationTestPlugin()
    {
        const auto failure = qgetenv("PRECESS_TEST_SETUP_CONSTRUCTION");
        if (failure == "standard")
            throw std::runtime_error("expected plugin construction failure");
        if (failure == "unknown")
            throw 42;
    }

private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override
    {
        return HandlerCreatorDestroyerFactory<SetupNavigationTestHandler, FeatureHandler>::get();
    }
};
}

#include "SetupNavigationTestPlugin.moc"
