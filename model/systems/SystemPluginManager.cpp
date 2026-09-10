/**
 * @file SystemPluginManager.cpp
 * @author 张家僮(htxz_6a6@163.com)
 */
#include "SystemPluginManager.h"
#include "SystemRegisterBase.h"
#include "PluginBase.h"

#include <QFileInfo>
#include <QPluginLoader>
#include <spdlog/spdlog.h>

Q_DECLARE_INTERFACE(systems::PluginBase, "com.PreCess.systems.PluginBase/1.0")

namespace systems {
void SystemPluginManager::registerStaticPlugins()
{
    const auto static_plugins = QPluginLoader::staticPlugins();
    for (const QStaticPlugin& static_plugin : static_plugins) {
        QObject* plugin_instance = static_plugin.instance();
        if (!qobject_cast<PluginBase*>(plugin_instance)) {
            continue;
        }

        QJsonObject meta_data = static_plugin.metaData();
        QString iid = meta_data.value("IID").toString();
        qsizetype slash_pos = iid.indexOf(u'/');
        QString iid_body = slash_pos >= 0 ? iid.left(slash_pos) : iid;
        qsizetype dot_pos = iid_body.lastIndexOf(u'.');
        std::string plugin_name = (dot_pos >= 0 ? iid_body.mid(dot_pos + 1) : iid_body).toStdString();

        QJsonObject system_meta_data = meta_data.value("MetaData").toObject();
        this->loadPlugin(system_meta_data, *plugin_instance, plugin_name);
    }
}

bool SystemPluginManager::registerPlugin(const std::filesystem::path& plugin_path)
{
    std::string plugin_name = plugin_path.stem().string();
    if (this->plugin_names_.count(plugin_name)) {
        spdlog::warn("Plugin already registered, no need to register again");
        return false;
    }

    // 读取插件元数据，找对应的系统注册器注册
    QString plugin_path_q = QString::fromLocal8Bit(plugin_path.string());
    QPluginLoader plugin_loader(plugin_path_q);
    QObject* plugin_instance = plugin_loader.instance();
    if (!plugin_instance) {
        spdlog::error("Failed to load plugin: {}", plugin_loader.errorString().toStdString());
        return false;
    }

    QJsonObject system_meta_data = plugin_loader.metaData().value("MetaData").toObject();
    return this->loadPlugin(system_meta_data, *plugin_instance, plugin_name);
}

bool SystemPluginManager::loadPlugin(const QJsonObject& system_meta_data, QObject& plugin_instance, const std::string& plugin_name)
{
    if (this->plugin_names_.count(plugin_name)) {
        spdlog::warn("Plugin '{}' already registered, no need to register again", plugin_name);
        return false;
    }

    std::string system_name = system_meta_data.value("system").toString().toStdString();
    if (!this->system_registers_.count(system_name)) {
        spdlog::error("System '{}' not found when registering system", system_name);
        return false;
    }

    // 插件对象实例化并获取插件的处理器 Handler
    PluginBase* plugin = qobject_cast<PluginBase*>(&plugin_instance);
    if (!plugin) {
        spdlog::error("Plugin '{}' does not inherit from PluginBase", plugin_name);
        return false;
    }

    SystemRegisterBase* cur_system = this->system_registers_[system_name].get();
    QJsonObject handler_data = system_meta_data.value("handler").toObject();
    if (cur_system->registerPlugin(handler_data, *plugin)) {
        this->plugin_names_.insert(plugin_name);
        return true;
    }

    return false;
}

void SystemPluginManager::unregisterPlugin(const std::filesystem::path& plugin_path)
{
    std::string plugin_name = plugin_path.stem().string();
    if (!this->plugin_names_.count(plugin_name)) {
        spdlog::warn("Plugin '{}' is not registered, cannot unregister", plugin_name);
        return;
    }

    // 读取插件元数据，找对应的系统注册器执行注销
    QString plugin_path_q = QString::fromStdString(plugin_path.string());
    QPluginLoader plugin_loader(plugin_path_q);
    QJsonObject system_meta_data = plugin_loader.metaData().value("MetaData").toObject();
    std::string system_name = system_meta_data.value("system").toString().toStdString();
    if (!this->system_registers_.count(system_name)) {
        spdlog::error("System '{}' not found when unregistering system", system_name);
        return;
    }

    SystemRegisterBase* cur_system = this->system_registers_[system_name].get();
    QJsonObject handler_data = system_meta_data.value("handler").toObject();

    cur_system->unregisterPlugin(handler_data);
    this->plugin_names_.erase(plugin_name);
}

std::vector<std::string> SystemPluginManager::getPluginNames() const
{
    return { this->plugin_names_.begin(), this->plugin_names_.end() };
}

bool SystemPluginManager::addSystemRegister(const std::string& system_name, std::unique_ptr<SystemRegisterBase> system_register)
{
    if (this->system_registers_.count(system_name))
    {
        spdlog::error("system {} has been registered", system_name);
        return false;
    }
    this->system_registers_[system_name] = std::move(system_register);
    spdlog::info("registering system {}", system_name);
    return true;
}

void SystemPluginManager::removeSystemRegister(const std::string& system_name)
{
    this->system_registers_.erase(system_name);
    spdlog::info("unregistering system {}", system_name);
}
}
