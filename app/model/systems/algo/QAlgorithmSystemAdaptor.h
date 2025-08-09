#ifndef Q_ALGORITHM_SYSTEM_ADAPTOR_H
#define Q_ALGORITHM_SYSTEM_ADAPTOR_H

#include "Core.h"
#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>

class QArgObject;
class QAlgorithmInfo;
namespace systems::algo {
class AlgorithmSystem;

/**
 * @brief 算法系统的QML适配器，负责C++的算法系统功能暴露给QML、Qt类型与C++类型的转换
 */
class QAlgorithmSystemAdaptor : public QObject {
    Q_OBJECT
    QML_ELEMENT
public:
    QAlgorithmSystemAdaptor(AlgorithmSystem& algo_system);
    /**
     * @brief 调用算法，debug模式下调用算法前检查参数类型是否正确
     * @param unique_name 算法唯一名称
     * @param args 算法参数
     * @return 算法结果
     */
    Q_INVOKABLE QVariant call(const QString& unique_name, Index model, const QList<QArgObject*>& args);

    /**
     * @brief 由qml获取所有已注册的算法功能和参数列表
     * @return 注册的算法功能和参数列表
     */
    Q_INVOKABLE QList<QAlgorithmInfo*> getAlgorithmsInfo() const;

private:
    AlgorithmSystem* algo_system_; // 算法系统的引用
};
}

#endif