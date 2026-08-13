#ifndef Q_ARG_OBJECT_H
#define Q_ARG_OBJECT_H
#include "ArgObject.h"
#include "QArgType.h"

#include <QVariant>

/**
 * @brief QML中使用的参数类型实例对象值，用于参数列表中的model。是对应QArgType描述的参数类型的变量实例，封装了QArgType参数类型和参数值QVariant
 */
class QArgObject : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("QArgObject instances are created by C++")

    Q_PROPERTY(const QArgType* type READ type CONSTANT)
    Q_PROPERTY(QVariant value MEMBER value_ WRITE setValue NOTIFY valueChanged)
public:
    QArgObject(const QArgType& type, QObject* parent = nullptr);
    /** 参数类型 */
    const QArgType* type() const;
    /** 获取参数值对应的参数对象 */
    std::optional<core::ArgObject> getValue() const;
    /** 设置参数值 */
    void setValue(const QVariant& value);

signals:
    void valueChanged();

private:
    const QArgType* type_ {};
    QVariant value_;
};

#endif // Q_ARG_OBJECT_H