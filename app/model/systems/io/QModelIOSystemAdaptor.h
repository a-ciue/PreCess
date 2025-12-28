#ifndef Q_MODEL_IO_SYSTEM_ADAPTOR_H
#define Q_MODEL_IO_SYSTEM_ADAPTOR_H

#include "Core.h"
#include <QUrl>
#include <QVariantList>
#include <QtQmlIntegration/qqmlintegration.h>

class QArgObject;
namespace systems::io {
class ModelIOSystem;
class QModelIOInfo;

/**
 * @brief 向Qml暴露的ModelIOSystem适配器，向界面暴露IO功能：读取模型、写出模型等各种功能操作
 */
class QModelIOSystemAdaptor : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QStringList dialogNameFilters READ getDialogNameFilters NOTIFY dialogNameFiltersChanged)
public:
    QModelIOSystemAdaptor(ModelIOSystem& io_system);
    /**
     * @brief 按照给定文件类型，读取指定路径的文件
     * @param unique_name 文件类型唯一名称
     * @param url 文件路径
     * @param args 附加参数列表
     * @return 是否读取成功
     */
    Q_INVOKABLE bool read(const QString& unique_name, const QUrl& url, const QVariantList& args);
    /**
     * @brief 按照给定文件类型，写出指定模型到指定路径的文件
     * @param unique_name 文件类型唯一名称
     * @param model 模型索引
     * @param url 文件路径
     * @param args 附加参数列表
     * @return 是否写出成功
     */
    Q_INVOKABLE bool write(const QString& unique_name, Index model, const QUrl& url, const QVariantList& args);
    /**
     * @brief 获取所有支持的文件类型信息
     * @return 文件类型信息列表
     */
    Q_INVOKABLE QList<QModelIOInfo*> getModelIOInfo() const;
    /**
     * @brief 供文件对话框使用，获取所有支持的文件类型过滤器
     * @return 文件类型过滤器列表，如 ["Wavefront .obj file (*.obj)", "All files (*)"]
     */
    QStringList getDialogNameFilters() const;

signals:
    void dialogNameFiltersChanged();

private:
    ModelIOSystem* io_system_; //> 文件系统的引用
};
}

#endif