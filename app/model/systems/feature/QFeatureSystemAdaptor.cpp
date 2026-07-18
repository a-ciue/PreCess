#include "QFeatureSystemAdaptor.h"
#include "FeatureEvents.h"
#include "FeatureParams.h"
#include "FeatureSystem.h"
#include "QArgObject.h"
#include "QFeatureInfo.h"
#include <spdlog/spdlog.h>

namespace systems::feature {
QFeatureSystemAdaptor::QFeatureSystemAdaptor(FeatureSystem& feature_system)
    : feature_system_(&feature_system)
{
    feature_system.setOnFeatureInfosChanged([this]() {
        emit featuresInfoChanged();
    });
}

QVariant QFeatureSystemAdaptor::invoke(const QString& unique_name)
{
    feature_system_->invoke(unique_name.toStdString());
    return {};
}

bool QFeatureSystemAdaptor::setParameter(const QString& unique_name, int index, const QVariant& value)
{
    const FeatureParams* params = feature_system_->params(unique_name.toStdString());
    if (!params || index < 0 || static_cast<std::size_t>(index) >= params->count()) {
        spdlog::error("QFeatureSystemAdaptor::setParameter: param {} of feature {} not found", index, unique_name.toStdString());
        return false;
    }
    // 按声明的参数类型把QVariant转换为ArgObject
    QArgType q_type(params->types()[index]);
    QArgObject q_object(q_type);
    q_object.setValue(value);
    if (std::optional arg = q_object.getValue()) {
        return feature_system_->setParameter(unique_name.toStdString(), static_cast<std::size_t>(index), std::move(*arg));
    }
    spdlog::error("QFeatureSystemAdaptor::setParameter: param {} of feature {} not valid", index, unique_name.toStdString());
    return false;
}

bool QFeatureSystemAdaptor::postKeyEvent(int key, int modifiers, bool pressed)
{
    return feature_system_->dispatchKeyEvent(KeyEvent { key, modifiers, pressed });
}

void QFeatureSystemAdaptor::setActiveModel(int id)
{
    active_model_id_ = id;
}

void QFeatureSystemAdaptor::setActiveComponent(int id)
{
    active_component_id_ = id;
}

QList<QFeatureInfo*> QFeatureSystemAdaptor::getFeaturesInfo() const
{
    QList<QFeatureInfo*> infos;
    for (const FeatureInfo* feature_info : feature_system_->getFeatureInfos()) {
        // 菜单归属取第一个菜单贡献项，未声明时归入默认"功能"菜单
        QString menu_path;
        if (!feature_info->menus.empty()) {
            menu_path = QString::fromStdString(feature_info->menus.front().menu_path);
        }
        if (menu_path.isEmpty()) {
            menu_path = QStringLiteral("功能");
        }
        QList<QArgType*> args;
        for (const auto& arg_type : feature_info->arg_types) {
            args << new QArgType(arg_type);
        }
        infos.append(new QFeatureInfo(
            QString::fromStdString(feature_info->name),
            QString::fromStdString(feature_info->display_name),
            QString::fromStdString(feature_info->description),
            std::move(menu_path),
            std::move(args)));
    }
    return infos;
}

std::optional<Index> QFeatureSystemAdaptor::activeModel() const
{
    if (active_model_id_ < 0) {
        return std::nullopt;
    }
    return active_model_id_;
}

std::optional<Index> QFeatureSystemAdaptor::activeComponent() const
{
    if (active_component_id_ < 0) {
        return std::nullopt;
    }
    return active_component_id_;
}
}
