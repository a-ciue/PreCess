#include "QSystemPluginManager.h"
#include "SystemPluginManager.h"

#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <filesystem>

namespace systems {

QSystemPluginManager::QSystemPluginManager(SystemPluginManager* system_plugin_manager_)
    : system_plugin_manager_(system_plugin_manager_)
{
    std::vector<std::string> names = system_plugin_manager_->getPluginNames();
    for (const auto& name : names) {
        plugin_names_.append(QString::fromStdString(name));
    }
}

bool QSystemPluginManager::registerPlugin(const QUrl& plugin_path)
{
    QString plugin_path_temp = plugin_path.toLocalFile();
    std::filesystem::path path_std = QFileInfo(plugin_path_temp).filesystemFilePath();
    bool result = system_plugin_manager_->registerPlugin(path_std);
    if (result) {
        plugin_paths_.append(plugin_path_temp);
        plugin_names_.append(QFileInfo(plugin_path_temp).fileName());
        emit pluginNamesChanged();
    }
    return result;
}

void QSystemPluginManager::unregisterPlugin(const QString& plugin_path)
{
    std::filesystem::path path_std = QFileInfo(plugin_path).filesystemFilePath();
    system_plugin_manager_->unregisterPlugin(path_std);
    plugin_paths_.removeOne(plugin_path);
    plugin_names_.removeOne(QFileInfo(plugin_path).fileName());
    emit pluginNamesChanged();
}

const QStringList& QSystemPluginManager::getPluginNames() const
{
    return plugin_names_;
}

QString QSystemPluginManager::getPluginPath(const QString& plugin_name) const
{
    for (const QString& path_str : plugin_paths_) {
        QFileInfo fileInfo(path_str);
        if (fileInfo.fileName() == plugin_name) {
            return path_str;
        }
    }

    return QString();
}
} // namespace system