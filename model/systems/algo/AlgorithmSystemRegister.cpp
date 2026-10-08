/**
 * @file AlgorithmSystemRegister.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "AlgorithmSystemRegister.h"
#include "AlgorithmSystem.h"
#include "PluginBase.h"

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
    // 创建插件处理器
    auto handler = plugin.makeHandler<AlgorithmSystem::SystemHandler>();
    if (!handler) {
        spdlog::error("Failed to create AlgorithmSystem::SystemHandler from plugin.");
        return false;
    }
    // 转换元数据
    HandlerMetaData handler_data = toMetaData(meta_data);
    // 注册处理器
    return this->system_->registerHandler(handler_data, std::move(handler));
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
    const auto navigation = meta_data.value("navigation").toObject();
    for (const auto& category : navigation.value("categories").toArray()) {
        if (category.isString()) {
        const auto category_name = category.toString().trimmed();
        if (!category_name.isEmpty())
            handle_data.navigation.categories.push_back(category_name.toStdString());
            continue;
        }
        if (!category.isObject())
            continue;
        const auto declaration = category.toObject();
        const auto id = declaration.value("id").toString().trimmed();
        const auto title = declaration.value("title").toString().trimmed();
        if (id.isEmpty() || id == "other" || title.isEmpty()) {
            spdlog::warn("Invalid navigation category declaration in algorithm '{}'", handle_data.name);
            continue;
        }
        handle_data.navigation.categories.push_back(id.toStdString());
        handle_data.navigation.category_definitions.push_back({ id.toStdString(), title.toStdString(),
            declaration.value("icon").toString().trimmed().toStdString(), declaration.value("order").toInt() });
    }
    handle_data.navigation.group = navigation.value("group").toString().toStdString();
    handle_data.navigation.icon = navigation.value("icon").toString().toStdString();
    handle_data.navigation.order = navigation.value("order").toInt();
    handle_data.navigation.label = navigation.value("label").toString().trimmed().toStdString();
    const auto category_defaults = navigation.value("category_defaults").toObject();
    for (auto category = category_defaults.begin(); category != category_defaults.end(); ++category) {
        const auto defaults = category.value().toObject();
        for (auto parameter = defaults.begin(); parameter != defaults.end(); ++parameter) {
            const auto value = parameter.value();
            if (value.isString() || value.isDouble() || value.isBool())
                handle_data.navigation.category_defaults[category.key().toStdString()][parameter.key().toStdString()]
                    = value.toVariant().toString().toStdString();
        }
    }
    return handle_data;
}
}
