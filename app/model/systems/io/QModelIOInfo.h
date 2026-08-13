#ifndef Q_MODEL_IO_INFO_H
#define Q_MODEL_IO_INFO_H
#include "QArgType.h"
#include <QObject>
#include <qqmlintegration.h>

namespace systems::io {
class ModelIOSystem;

/**
 * @brief 向Qml暴露的受支持的文件类型信息，Qml据此提示用户以文件类型、拓展名
 */
class QModelIOInfo : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("QModelIOInfo instances are created by C++")
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(QStringList extensions READ extensions CONSTANT)
    Q_PROPERTY(QList<QArgType*> read_arg_types READ readArgTypes CONSTANT)
    Q_PROPERTY(QList<QArgType*> write_arg_types READ writeArgTypes CONSTANT)
public:
    QModelIOInfo(QString name, QString description, const std::vector<std::string>& extensions, QList<QArgType*> read_arg_types, QList<QArgType*> write_arg_types, QObject* parent = nullptr)
        : QObject(parent)
        , name_(std::move(name))
        , description_(std::move(description))
        , read_arg_types_(std::move(read_arg_types))
        , write_arg_types_(std::move(write_arg_types))
    {
        for (const auto& ext : extensions) {
            extensions_ << QString::fromStdString(ext);
        }
    }
    QString name() const { return name_; }
    QString description() const { return description_; }
    QStringList extensions() const { return extensions_; }
    QList<QArgType*> readArgTypes() const { return read_arg_types_; }
    QList<QArgType*> writeArgTypes() const { return write_arg_types_; }

private:
    QString name_; // 算法唯一名称，用作索引
    QString description_; // 算法描述
    QStringList extensions_; //> 文件类型对应拓展名列表
    QList<QArgType*> read_arg_types_; //> 读操作参数类型列表
    QList<QArgType*> write_arg_types_; //> 写操作参数类型列表
};
}
#endif // !Q_MODEL_IO_INFO_H
