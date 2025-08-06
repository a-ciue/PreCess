#ifndef Q_ARG_OBJECT_H
#define Q_ARG_OBJECT_H
#include <QObject>
#include <any>
#include <QFileInfo>

#include "ArgType.h"
#include "QArgType.h"

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
    Q_PROPERTY(QVariant value MEMBER value_)
public:
    QArgObject(ArgType&& type, QObject* parent = nullptr);
    QArgObject(const ArgType& type, QObject* parent = nullptr);

    QArgType::ArgTypeEnum type() const;
    QString name() const;
    QString content() const;
    QString desc() const;

    std::optional<std::any> getValue() const;

private:
    const ArgType& type_;
    QVariant value_;
};

#endif // Q_ARG_OBJECT_H