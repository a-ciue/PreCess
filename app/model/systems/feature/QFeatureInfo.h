#ifndef Q_FEATURE_INFO_H
#define Q_FEATURE_INFO_H
#include "QArgType.h"
#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <qqmlintegration.h>

/**
 * @brief 向 QML 暴露功能参数、执行身份及关联入口，菜单位置由入口声明。
 */
class QFeatureInfo : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("QFeatureInfo instances are created by C++")
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString display_name READ displayName CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(QString icon READ icon CONSTANT)
    Q_PROPERTY(QList<QArgType*> arg_types READ argTypes CONSTANT)
    Q_PROPERTY(QStringList entry_ids READ entryIds CONSTANT)
    Q_PROPERTY(int order READ order CONSTANT)
    Q_PROPERTY(QString label READ label CONSTANT)
    Q_PROPERTY(QVariantMap category_defaults READ entryDefaults CONSTANT)
    Q_PROPERTY(bool interactive READ interactive CONSTANT)
public:
    QFeatureInfo(QString name, QString display_name, QString description, QString icon, QList<QArgType*> arg_types,
        bool interactive = false, QObject* parent = nullptr,
        QStringList entry_ids = {}, int order = 0, QString label = {}, QVariantMap category_defaults = {})
        : QObject(parent)
        , name_(std::move(name))
        , display_name_(std::move(display_name))
        , description_(std::move(description))
        , icon_(std::move(icon))
        , arg_types_(std::move(arg_types))
        , interactive_(interactive)
        , entry_ids_(std::move(entry_ids))
        , order_(order)
        , label_(std::move(label))
        , category_defaults_(std::move(category_defaults))
    {
    }
    QString name() const { return name_; }
    QString displayName() const { return display_name_; }
    QString description() const { return description_; }
    QString icon() const { return icon_; }
    QList<QArgType*> argTypes() const { return arg_types_; }
    bool interactive() const { return interactive_; }
    QStringList entryIds() const { return entry_ids_; }
    int order() const { return order_; }
    QString label() const { return label_.isEmpty() ? display_name_ : label_; }
    QVariantMap entryDefaults() const { return category_defaults_; }

private:
    QString name_; //> 功能唯一名称，用作索引
    QString display_name_; //> 功能UI展示用名称
    QString description_; //> 功能描述
    QString icon_; //> 自定义图标的 qrc 资源路径，为空时使用通用插件图标
    QList<QArgType*> arg_types_; //> 功能参数类型列表
    QStringList entry_ids_;
    int order_ = 0;
    QString label_;
    QVariantMap category_defaults_;
    bool interactive_ = false; //> 是否声明视口交互能力
};
#endif // !Q_FEATURE_INFO_H
