#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQmlIntegration/qqmlintegration.h>

namespace systems {
class SystemPluginManager;

class QSystemPluginManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(const QStringList& pluginNames READ getPluginNames NOTIFY pluginNamesChanged)
public:
    QSystemPluginManager(SystemPluginManager* system_plugin_manager_);

    Q_INVOKABLE bool registerPlugin(const QString& plugin_path);

    Q_INVOKABLE void unregisterPlugin(const QString& plugin_path);

    const QStringList& getPluginNames() const;

    Q_INVOKABLE QString getPluginPath(const QString& plugin_name) const;

private:
    void updatePluginNames();

signals:
    void pluginNamesChanged();

private:
    SystemPluginManager* system_plugin_manager_;
    QStringList plugin_paths_;
    QStringList plugin_names_;
};
}
