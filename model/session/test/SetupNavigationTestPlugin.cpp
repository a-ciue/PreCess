/**
 * @file SetupNavigationTestPlugin.cpp
 * @brief 验证可选 setup 接口经真实 DLL 边界注册，不安装到产品插件目录。
 */
#include "AlgorithmHandler.h"
#include "AlgorithmRegistrar.h"
#include "HandlerCreatorDestroyerFactory.h"
#include "PluginBase.h"

#include <QObject>

namespace systems::algo {
class SetupNavigationTestHandler : public AlgorithmHandler {
public:
    void setup(AlgorithmRegistrar& registrar) override
    {
        registrar.addCategory({ "setup-custom", "Setup custom category", "", 5 });
        registrar.setLabel("Setup algorithm");
        registrar.setOrder(7);
        registrar.setCategoryDefault("setup-custom", "Size", "3");
    }
    std::vector<core::ArgType> args_type() const override
    {
        return { { ArgTypeEnum::Float, "Size", "1", "" } };
    }
    std::any execute(HandlerContext&, const std::vector<core::ArgObject>&) override { return {}; }
};

class SetupNavigationTestPlugin : public QObject, public PluginBase {
    Q_OBJECT
    Q_INTERFACES(systems::PluginBase)
    Q_PLUGIN_METADATA(IID "com.PreCess.systems.algo.SetupNavigationTestPlugin/1.0" FILE "SetupNavigationTestPlugin.json")
private:
    const HandlerCreatorDestroyer& getHandlerCreatorDestroyer() noexcept override
    {
        return HandlerCreatorDestroyerFactory<SetupNavigationTestHandler, AlgorithmHandler>::get();
    }
};
}

#include "SetupNavigationTestPlugin.moc"
