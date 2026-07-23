#include "QFeatureSystemAdaptor.h"
#include "FeatureEvents.h"
#include "FeatureParams.h"
#include "FeatureSystem.h"
#include "QArgObject.h"
#include "QFeatureInfo.h"
#include <spdlog/spdlog.h>

#include <any>
#include <string>
#include <vector>

namespace systems::feature {
namespace {
//! @brief 功能结果 std::any → QVariant
QVariant anyToQVariant(const std::any& value)
{
    if (!value.has_value())
        return {};

    if (value.type() == typeid(std::string))
        return QString::fromStdString(std::any_cast<std::string>(value));

    if (value.type() == typeid(double))
        return std::any_cast<double>(value);

    if (value.type() == typeid(long long))
        return static_cast<qlonglong>(std::any_cast<long long>(value));

    if (value.type() == typeid(int))
        return std::any_cast<int>(value);

    if (value.type() == typeid(bool))
        return std::any_cast<bool>(value);

    if (value.type() == typeid(std::vector<double>)) {
        const auto& vec = std::any_cast<const std::vector<double>&>(value);
        QVariantList list;
        list.reserve((int)vec.size());
        for (double v : vec)
            list.append(v);
        return list;
    }

    return {};
}
}

QFeatureSystemAdaptor::QFeatureSystemAdaptor(FeatureSystem& feature_system)
    : feature_system_(&feature_system)
{
    feature_system.setOnFeatureInfosChanged([this]() {
        emit featuresInfoChanged();
    });
}

QVariant QFeatureSystemAdaptor::invoke(const QString& unique_name)
{
    return anyToQVariant(feature_system_->invoke(unique_name.toStdString()));
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
        // 每个菜单贡献项生成一条功能信息（同一功能可挂到多个菜单），未声明时归入默认"功能"菜单
        std::vector<MenuContribution> menus = feature_info->menus;
        if (menus.empty()) {
            menus.push_back({ "功能", "", "" });
        }
        for (const auto& menu : menus) {
            QList<QArgType*> args;
            for (const auto& arg_type : feature_info->arg_types) {
                args << new QArgType(arg_type);
            }
            infos.append(new QFeatureInfo(
                QString::fromStdString(feature_info->name),
                QString::fromStdString(feature_info->display_name),
                QString::fromStdString(feature_info->description),
                QString::fromStdString(menu.menu_path.empty() ? "功能" : menu.menu_path),
                QString::fromStdString(menu.icon),
                std::move(args)));
        }
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
