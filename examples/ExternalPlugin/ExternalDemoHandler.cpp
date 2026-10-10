#include "ExternalDemoHandler.h"
#include "FeatureContext.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"

#include <spdlog/spdlog.h>

namespace systems::feature {
void ExternalDemoHandler::setup(FeatureRegistrar& reg, FeatureContext&)
{
    // 功能参数注册：UI 依声明生成控件，初始值取 ArgType::content
    reg.addParameter({ ArgTypeEnum::Float, "问候次数", "1.0", "执行时打印的演示参数" });
    // 菜单项注册："示例" 菜单分页默认分组
    reg.navigation().addEntry({ "ExternalDemo", "外部插件示例", "qrc:/images/toolbar/precess_extra_plugin.svg", 0, "示例" });
    spdlog::info("ExternalDemo: setup");
}

std::any ExternalDemoHandler::execute(FeatureContext& ctx)
{
    const auto* count = ctx.params.value(0).get<ArgTypeEnum::Float>();
    spdlog::info("ExternalDemo: hello from an external plugin, param={}", count ? *count : 0.0);
    return {};
}
}
