#ifndef Q_ARG_OBJECT_H
#define Q_ARG_OBJECT_H
#include "ArgObject.h"
#include "ArgType.h"
#include "QArgType.h"

#include <QVariant>

/**
 * @brief QML中使用的参数对象，用于ListView中的model。封装了ArgType参数类型和参数值QVariant
 */
class QArgObject : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QArgType::ArgTypeEnum type READ type CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString content READ content CONSTANT)
    Q_PROPERTY(QString description READ desc CONSTANT)
    Q_PROPERTY(QVariant value MEMBER value_ WRITE setValue NOTIFY valueChanged)
public:
    QArgObject(core::ArgType&& type, QObject* parent = nullptr);
    QArgObject(const core::ArgType& type, QObject* parent = nullptr);

    QArgType::ArgTypeEnum type() const;
    QString name() const;
    QString content() const;
    QString desc() const;

    std::optional<core::ArgObject> getValue() const;
    void setValue(const QVariant& value);

signals:
    void valueChanged();

private:
    const core::ArgType& type_;
    QVariant value_;
};

#endif // Q_ARG_OBJECT_H