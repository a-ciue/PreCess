#ifndef Q_ALGORITHM_INFO_H
#define Q_ALGORITHM_INFO_H
#include "QArgObject.h"
#include <QObject>
#include <qqmlintegration.h>

/**
 * @brief 向Qml暴露的算法信息，Qml据此构造算法按钮、输入框等算法调用参数UI
 */
class QAlgorithmInfo : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString display_name READ displayName CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(QList<QArgObject*> arg_types READ argTypes CONSTANT)
public:
    QAlgorithmInfo(QString name, QString display_name, QString description, QList<QArgObject*> arg_types, QObject* parent = nullptr)
        : QObject(parent)
        , name_(std::move(name))
        , display_name_(std::move(display_name))
        , description_(std::move(description))
        , arg_types_(std::move(arg_types))
    {
    }
    QString name() const { return name_; }
    QString displayName() const { return display_name_; }
    QString description() const { return description_; }
    QList<QArgObject*> argTypes() const { return arg_types_; }

private:
    QString name_; // 算法唯一名称，用作索引
    QString display_name_; // 算法UI展示用名称
    QString description_; // 算法描述
    QList<QArgObject*> arg_types_; // 算法参数类型列表
};
#endif // !Q_ALGORITHM_INFO_H
