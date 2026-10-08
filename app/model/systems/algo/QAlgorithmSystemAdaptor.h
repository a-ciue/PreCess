/** @file QAlgorithmSystemAdaptor.h
 * @brief 算法声明与调用的 Qt 适配；任务状态由 QTaskStatus 统一提供。
 */
#pragma once
#include "Core.h"
#include <QVariant>
#include <QtQmlIntegration/qqmlintegration.h>
class QAlgorithmInfo;
class QTaskStatus;
namespace systems::algo {
class AlgorithmSystem;
class QAlgorithmSystemAdaptor : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("QAlgorithmSystemAdaptor is provided by C++")
    Q_PROPERTY(QList<QAlgorithmInfo*> algorithmsInfo READ getAlgorithmsInfo NOTIFY algorithmsInfoChanged)
    Q_PROPERTY(QVariantList navigationCategories READ getNavigationCategories NOTIFY algorithmsInfoChanged)
public:
    QAlgorithmSystemAdaptor(AlgorithmSystem& system, QTaskStatus& status);
    ~QAlgorithmSystemAdaptor() override;
    Q_INVOKABLE void call(const QString& name, Index model, const QVariantList& args);
    QList<QAlgorithmInfo*> getAlgorithmsInfo() const;
    QVariantList getNavigationCategories() const;
signals:
    void algorithmsInfoChanged();

private:
    AlgorithmSystem* algo_system_;
    QTaskStatus* task_status_;
};
}
