#ifndef ARG_TYPE_OBJECT_H
#define ARG_TYPE_OBJECT_H

#include <QObject>

class ArgTypeObject : public QObject {
    Q_OBJECT

    Q_PROPERTY(int type MEMBER type NOTIFY typeChanged)
    Q_PROPERTY(QString name MEMBER name NOTIFY nameChanged)
    Q_PROPERTY(QString content MEMBER content NOTIFY contentChanged)
public:
    ArgTypeObject(int type, const QString& name, const QString& content, QObject* parent = nullptr)
        : QObject(parent)
        , type(type)
        , name(name)
        , content(content)
    {
    }

signals:
	void typeChanged();
    void nameChanged();
    void contentChanged();

private:
    int type;
    QString name;
    QString content;
};
#endif // !ARG_TYPE_OBJECT_H
