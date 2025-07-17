/**
 * @file ModelIOSystemRegister.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "ModelIOSystemRegister.h"
#include "ModelIOSystem.h"

#include <QJsonArray>
#include <assert.h>
#include <spdlog/spdlog.h>

namespace systems::io {
ModelIOSystemRegister::ModelIOSystemRegister(ModelIOSystem& system): system_(&system)
{
}

bool ModelIOSystemRegister::registerHandler(const QJsonObject& meta_data, std::any handler)
{
    assert(this->system_);
    using namespace std;

    auto handler_p = any_cast<shared_ptr<ModelIOSystem::Handler>>(&handler);
    if (!handler_p)
    {
        return false;
    }

    HandlerMetaData handler_data = toMetaData(meta_data);
    return this->system_->registerHandler(handler_data, *handler_p);
}

void ModelIOSystemRegister::unregisterHandler(const QJsonObject& meta_data)
{
    assert(this->system_);

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
