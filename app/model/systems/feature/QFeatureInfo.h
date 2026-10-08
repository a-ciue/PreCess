#ifndef Q_FEATURE_INFO_H
#define Q_FEATURE_INFO_H
#include "QArgType.h"
#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <qqmlintegration.h>

/**
 * @brief 向Qml暴露的功能信息，Qml据此构造功能菜单与参数面板
 */
class QFeatureInfo : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("QFeatureInfo instances are created by C++")
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString display_name READ displayName CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(QString menu_path READ menuPath CONSTANT)
    Q_PROPERTY(QString icon READ icon CONSTANT)
    Q_PROPERTY(QList<QArgType*> arg_types READ argTypes CONSTANT)
    Q_PROPERTY(QStringList categories READ categories CONSTANT)
    Q_PROPERTY(QString group READ group CONSTANT)
    Q_PROPERTY(int order READ order CONSTANT)
    Q_PROPERTY(QString label READ label CONSTANT)
    Q_PROPERTY(QVariantMap category_defaults READ categoryDefaults CONSTANT)
    Q_PROPERTY(bool interactive READ interactive CONSTANT)
public:
    QFeatureInfo(QString name, QString display_name, QString description, QString menu_path, QString icon, QList<QArgType*> arg_types,
        bool interactive = false, QObject* parent = nullptr,
        QStringList categories = {}, QString group = {}, int order = 0, QString label = {}, QVariantMap category_defaults = {})
        : QObject(parent)
        , name_(std::move(name))
        , display_name_(std::move(display_name))
        , description_(std::move(description))
        , menu_path_(std::move(menu_path))
        , icon_(std::move(icon))
        , arg_types_(std::move(arg_types))
        , interactive_(interactive)
        , categories_(std::move(categories))
        , group_(std::move(group))
        , order_(order)
        , label_(std::move(label))
        , category_defaults_(std::move(category_defaults))
    {
    }
    QString name() const { return name_; }
    QString displayName() const { return display_name_; }
    QString description() const { return description_; }
    QString menuPath() const { return menu_path_; }
    QString icon() const { return icon_; }
    QList<QArgType*> argTypes() const { return arg_types_; }
    bool interactive() const { return interactive_; }
    QStringList categories() const { return categories_; }
    QString group() const { return group_; }
    int order() const { return order_; }
    QString label() const { return label_.isEmpty() ? display_name_ : label_; }
    QVariantMap categoryDefaults() const { return category_defaults_; }

private:
    QString name_; //> 功能唯一名称，用作索引
    QString display_name_; //> 功能UI展示用名称
    QString description_; //> 功能描述
    QString menu_path_; //> 功能归属的菜单路径，以 '/' 分隔，约定两级（"菜单/分组"）：菜单为 ribbon 分页、分组为页内分组
    QString icon_; //> 自定义图标的 qrc 资源路径，为空时使用通用插件图标
    QList<QArgType*> arg_types_; //> 功能参数类型列表
    QStringList categories_;
    QString group_;
    int order_ = 0;
    QString label_;
    QVariantMap category_defaults_;
    bool interactive_ = false; //> 是否声明视口交互能力
};
#endif // !Q_FEATURE_INFO_H
