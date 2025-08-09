/**
 * @file ModelIOSystemRegister.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "ModelIOSystemRegister.h"
#include "ModelIOSystem.h"
#include "PluginHandler.h"

#include <QJsonArray>
#include <cassert>
#include <spdlog/spdlog.h>

namespace systems::io {
ModelIOSystemRegister::ModelIOSystemRegister(ModelIOSystem& system): system_(&system)
{
    assert(this->system_);
}

bool ModelIOSystemRegister::registerPlugin(const QJsonObject& meta_data, PluginBase& plugin)
{
    using namespace std;
    try
    {
		auto plugin_handler = std::make_unique<ModelIOSystem::PluginHandler>(plugin);

	    HandlerMetaData handler_data = toMetaData(meta_data);
	    return this->system_->registerHandler(handler_data, std::move(plugin_handler));
    } catch (const std::bad_any_cast& e)
    {
        spdlog::error("Failed to cast plugin to ModelIOSystem::Handler: {}", e.what());
        return false;
    } catch (const std::exception& e) {
        spdlog::error("Exception occurred while registering handler: {}", e.what());
        return false;
    }
}

void ModelIOSystemRegister::unregisterPlugin(const QJsonObject& meta_data)
{
    HandlerMetaData handler_data = toMetaData(meta_data);
    this->system_->unregisterHandler(handler_data);
}

HandlerMetaData ModelIOSystemRegister::toMetaData(const QJsonObject& meta_data) const
{
    HandlerMetaData handle_data;
    handle_data.file_type = meta_data.value("file_type").toString().toStdString();
    QJsonArray extensions = meta_data.value("extensions").toArray();
    for (const auto& ext : extensions)
    {
        handle_data.extensions.push_back(ext.toString().toStdString());
    }
    return handle_data;
}
}
