#ifndef Q_EDIT_SYSTEM_ADAPTOR_H
#define Q_EDIT_SYSTEM_ADAPTOR_H

#include "Core.h"
#include <QVariant>
#include <QtQmlIntegration/qqmlintegration.h>

class QArgObject;
class QEditInfo;
namespace systems::edit {
class EditSystem;

/**
 * @brief 模型编辑系统的QML适配器，负责C++的编辑系统功能暴露给QML、Qt类型与C++类型的转换
 */
class QEditSystemAdaptor : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QList<QEditInfo*> editsInfo READ getEditsInfo NOTIFY editInfoChanged)
public:
    QEditSystemAdaptor(EditSystem& edit_system);
    /**
     * @brief 调用编辑功能
     * @param unique_name 编辑功能唯一名称
     * @param args 编辑参数
     * @return 结果
     */
    Q_INVOKABLE QVariant call(const QString& unique_name, Index model, const QVariantList& args);

    /**
     * @brief 获取所有已注册的编辑功能和参数列表
     * @return 注册的编辑功能和参数列表
     */
    Q_INVOKABLE QList<QEditInfo*> getEditsInfo() const;

signals:
    void editInfoChanged();

private:
    EditSystem* edit_system_; //> 编辑系统的引用
};
}

#endif