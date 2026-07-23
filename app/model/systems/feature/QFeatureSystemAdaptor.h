#ifndef Q_FEATURE_SYSTEM_ADAPTOR_H
#define Q_FEATURE_SYSTEM_ADAPTOR_H

#include "Core.h"
#include <QVariant>
#include <QtQmlIntegration/qqmlintegration.h>
#include <optional>

class QFeatureInfo;
namespace systems::feature {
class FeatureSystem;

/**
 * @brief 功能系统的QML适配器，负责C++的功能系统功能暴露给QML、Qt类型与C++类型的转换，
 * 并承接UI产生的按键事件与活动模型状态同步
 */
class QFeatureSystemAdaptor : public QObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QList<QFeatureInfo*> featuresInfo READ getFeaturesInfo NOTIFY featuresInfoChanged)
public:
    QFeatureSystemAdaptor(FeatureSystem& feature_system);
    /**
     * @brief 菜单触发的功能调用
     * @param unique_name 功能唯一名称
     */
    Q_INVOKABLE QVariant invoke(const QString& unique_name);
    /**
     * @brief 修改功能参数，实时生效并广播参数变更事件
     * @param unique_name 功能唯一名称
     * @param index 参数在参数列表中的下标
     * @param value 新参数值
     */
    Q_INVOKABLE bool setParameter(const QString& unique_name, int index, const QVariant& value);
    /**
     * @brief UI层向功能系统派发按键事件
     * @return 事件已被功能消费（应 accept）时为 true
     */
    Q_INVOKABLE bool postKeyEvent(int key, int modifiers, bool pressed);
    /**
     * @brief QML侧同步当前活动模型id，供功能上下文动态获取
     */
    Q_INVOKABLE void setActiveModel(int id);
    /**
     * @brief QML侧同步当前活动组件id，供功能上下文动态获取
     */
    Q_INVOKABLE void setActiveComponent(int id);

    /**
     * @brief 由qml获取所有已注册的功能和参数列表
     */
    QList<QFeatureInfo*> getFeaturesInfo() const;

    /**
     * @brief 获取底层功能系统指针（interaction 交互状态等系统级接口用）
     */
    FeatureSystem* featureSystem() const { return feature_system_; }

    // 功能上下文 provider 的后端实现，由 QModelManager 注入 FeatureSystem
    std::optional<Index> activeModel() const;
    std::optional<Index> activeComponent() const;

signals:
    void featuresInfoChanged();

private:
    FeatureSystem* feature_system_; //> 功能系统的引用
    Index active_model_id_ { -1 };
    Index active_component_id_ { -1 };
};
}

#endif
