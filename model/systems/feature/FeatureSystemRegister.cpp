/**
 * @file FeatureSystemRegister.cpp
 */
#include "FeatureSystemRegister.h"
#include "FeatureSystem.h"
#include "PluginBase.h"

#include <cassert>
#include <spdlog/spdlog.h>

namespace systems::feature {
FeatureSystemRegister::FeatureSystemRegister(FeatureSystem& system)
    : system_(&system)
{
    assert(this->system_);
}

bool FeatureSystemRegister::registerPlugin(const QJsonObject& meta_data, PluginBase& plugin)
{
    // 创建插件处理器
    auto handler = plugin.makeHandler<FeatureSystem::SystemHandler>();
    if (!handler) {
        spdlog::error("Failed to create FeatureSystem::SystemHandler from plugin.");
        return false;
    }
    // 转换元数据并注册处理器
    auto md = toMetaData(meta_data);
    return system_->registerHandler(md, std::move(handler));
}

void FeatureSystemRegister::unregisterPlugin(const QJsonObject& meta_data)
{
    system_->unregisterHandler(toMetaData(meta_data));
}

HandlerMetaData FeatureSystemRegister::toMetaData(const QJsonObject& meta_data) const
{
    HandlerMetaData handler_data;
    handler_data.name = meta_data.value("name").toString().toStdString();
    handler_data.display_name = meta_data.value("display_name").toString().toStdString();
    handler_data.description = meta_data.value("description").toString().toStdString();
    handler_data.result_display = meta_data.value("result_display").toString().toStdString();
    handler_data.interactive = meta_data.value("interactive").toBool(false);
    return handler_data;
}
}
