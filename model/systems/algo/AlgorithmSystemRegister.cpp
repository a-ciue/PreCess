/**
 * @file AlgorithmSystemRegister.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "AlgorithmSystemRegister.h"
#include "PluginHandler.h"
#include "AlgorithmSystem.h"
#include <QJsonArray>
#include <cassert>
#include <spdlog/spdlog.h>

namespace systems::algo {
AlgorithmSystemRegister::AlgorithmSystemRegister(AlgorithmSystem& system)
    : system_(&system)
{
    assert(this->system_);
}

bool AlgorithmSystemRegister::registerPlugin(const QJsonObject& meta_data, PluginBase& plugin)
{
    using namespace std;
    try {
        // 创建插件处理器
        auto plugin_handler = std::make_unique<AlgorithmSystem::PluginHandler>(plugin);
        // 转换元数据
        HandlerMetaData handler_data = toMetaData(meta_data);
        // 注册处理器
        return this->system_->registerHandler(handler_data, std::move(plugin_handler));
    } catch (const std::bad_any_cast& e) {
        spdlog::error("Failed to cast plugin to AlgorithmSystem::Handler: {}", e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Exception occurred while registering handler: {}", e.what());
        return false;
    }
}

void AlgorithmSystemRegister::unregisterPlugin(const QJsonObject& meta_data)
{
    auto handler_data = toMetaData(meta_data);
    system_->unregisterHandler(handler_data);
}

HandlerMetaData AlgorithmSystemRegister::toMetaData(const QJsonObject& meta_data) const
{
    HandlerMetaData handle_data;
    handle_data.name = meta_data.value("name").toString().toStdString();
    handle_data.display_name = meta_data.value("display_name").toString().toStdString();
    return handle_data;
}
}
