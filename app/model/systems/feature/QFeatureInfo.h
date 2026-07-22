#ifndef Q_FEATURE_INFO_H
#define Q_FEATURE_INFO_H
#include "QArgType.h"
#include <QObject>
#include <qqmlintegration.h>

/**
 * @brief 向Qml暴露的功能信息，Qml据此构造功能菜单与参数面板
 */
class QFeatureInfo : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString display_name READ displayName CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(QString menu_path READ menuPath CONSTANT)
    Q_PROPERTY(QList<QArgType*> arg_types READ argTypes CONSTANT)
public:
    QFeatureInfo(QString name, QString display_name, QString description, QString menu_path, QList<QArgType*> arg_types, QObject* parent = nullptr)
        : QObject(parent)
        , name_(std::move(name))
        , display_name_(std::move(display_name))
        , description_(std::move(description))
        , menu_path_(std::move(menu_path))
        , arg_types_(std::move(arg_types))
    {
    }
    QString name() const { return name_; }
    QString displayName() const { return display_name_; }
    QString description() const { return description_; }
    QString menuPath() const { return menu_path_; }
    QList<QArgType*> argTypes() const { return arg_types_; }

private:
    QString name_; //> 功能唯一名称，用作索引
    QString display_name_; //> 功能UI展示用名称
    QString description_; //> 功能描述
    QString menu_path_; //> 功能归属的菜单路径，以 '/' 分隔，约定两级（"菜单/分组"）：菜单为 ribbon 分页、分组为页内分组
    QList<QArgType*> arg_types_; //> 功能参数类型列表
};
#endif // !Q_FEATURE_INFO_H
