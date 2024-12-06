#include <QtQml/qqmlregistration.h>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>

struct Presenter :public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QStringList sources READ sources CONSTANT)
    QStringList sources() const
    {
        return QStringList {} << "Cone"
                              << "Sphere"
                              << "Capsule";
    }
};
