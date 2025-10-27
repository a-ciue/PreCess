/**
 * @file ModelIOSystemRegister.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "ModelIOSystemRegister.h"
#include "ModelIOSystem.h"
#include "PluginBase.h"

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

    auto handler = plugin.makeHandler<ModelIOSystem::SystemHandler>();
    if (!handler) {
        spdlog::error("Failed to create ModelIOSystem::SystemHandler from plugin.");
        return false;
    }

    HandlerMetaData handler_data = toMetaData(meta_data);
    return this->system_->registerHandler(handler_data, std::move(handler));
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
