#include "QFeatureSystemAdaptor.h"
#include "FeatureParams.h"
#include "FeatureSystem.h"
#include "QArgObject.h"
#include "QFeatureInfo.h"
#include "QSelection.h"
#include <QQmlEngine>

#include <QMetaObject>
#include <spdlog/spdlog.h>

#include <any>
#include <string>
#include <vector>

namespace {
//! @brief 快照和事件共用类型转换，避免两条展示路径解释出不同的参数值。
QVariant parameterToQVariant(const core::ArgObject& value)
{
    // 将参数变更转换为面板展示值，保留各类型的实际载荷。
    QVariant q_value;
    if (const auto* v = value.get<ArgTypeEnum::Text>()) {
        q_value = QString::fromStdString(*v);
    } else if (const auto* v = value.get<ArgTypeEnum::Bool>()) {
        q_value = *v;
    } else if (const auto* v = value.get<ArgTypeEnum::Int>()) {
        q_value = static_cast<qlonglong>(*v);
    } else if (const auto* v = value.get<ArgTypeEnum::Float>()) {
        q_value = *v;
    } else if (const auto* v = value.get<ArgTypeEnum::Combo>()) {
        // Combo/Button 的底层载荷为 int，不能漏转为空值回写到面板。
        q_value = *v;
    } else if (const auto* v = value.get<ArgTypeEnum::Path>()) {
        const auto path = v->u8string();
        q_value = QString::fromUtf8(reinterpret_cast<const char*>(path.data()), static_cast<qsizetype>(path.size()));
    } else if (const auto* v = value.get<ArgTypeEnum::Selector>()) {
        auto* selection = new QSelection;
        selection->set(*v);
        // 参数数组持有包装对象；替换后交由 QML GC 回收，不随临时信号失效。
        QQmlEngine::setObjectOwnership(selection, QQmlEngine::JavaScriptOwnership);
        q_value = QVariant::fromValue(selection);
    }
    return q_value;
}
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

namespace systems::feature {

QFeatureSystemAdaptor::QFeatureSystemAdaptor(FeatureSystem& feature_system)
    : feature_system_(&feature_system)
{
    feature_system.setOnFeatureInfosChanged([this]() {
        emit featuresInfoChanged();
    });
}

QFeatureSystemAdaptor::~QFeatureSystemAdaptor() = default;

FeatureSystem* QFeatureSystemAdaptor::featureSystem() const
{
    return feature_system_;
}

QVariant QFeatureSystemAdaptor::invoke(const QString& unique_name)
{
    return anyToQVariant(feature_system_->invoke(unique_name.toStdString()));
}

void QFeatureSystemAdaptor::notifyParameterChanged(const std::string& feature, std::size_t index, const core::ArgObject& value)
{
    const QVariant q_value = parameterToQVariant(value);
    // 未支持的展示类型保留现有值，不能用无效 QVariant 清掉刚编辑的参数。
    if (!q_value.isValid())
        return;
    emit paramValueChanged(QString::fromStdString(feature), static_cast<int>(index), q_value);
}

QVariantList QFeatureSystemAdaptor::getParameterValues(const QString& unique_name) const
{
    const FeatureParams* params = feature_system_->params(unique_name.toStdString());
    if (!params)
        return {};
    QVariantList values;
    values.reserve(static_cast<qsizetype>(params->count()));
    for (std::size_t i = 0; i < params->count(); ++i)
        values.append(parameterToQVariant(params->value(i)));
    return values;
}

void QFeatureSystemAdaptor::notifyScalarAttributeDisplayRequested(
    Index component_id,
    const std::string& attribute_name)
{
    const QString q_attribute_name = QString::fromStdString(attribute_name);
    // FeatureSystem 在 execute 返回后统一 flush，排队发送可保证 QML 在模型刷新后设置渲染属性。
    QMetaObject::invokeMethod(this, [this, component_id, q_attribute_name]() { emit scalarAttributeDisplayRequested(component_id, q_attribute_name); }, Qt::QueuedConnection);
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

bool QFeatureSystemAdaptor::setFeatureActive(const QString& unique_name)
{
    return feature_system_->setFeatureActive(unique_name.toStdString());
}

void QFeatureSystemAdaptor::setActiveModel(int id)
{
    active_model_id_ = id;
}

void QFeatureSystemAdaptor::setActiveComponent(int id)
{
    active_component_id_ = id;
}

QVariantList QFeatureSystemAdaptor::getNavigationCategories() const
{
    QVariantList result;
    for (const auto& category : feature_system_->getNavigationCategories()) {
        result.append(QVariantMap { { "id", QString::fromStdString(category.id) },
            { "title", QString::fromStdString(category.title) },
            { "icon", QString::fromStdString(category.icon) }, { "order", category.order } });
    }
    return result;
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
        QStringList categories;
        for (const auto& category : feature_info->navigation.categories)
            categories.append(QString::fromStdString(category));
        QVariantMap category_defaults;
        for (const auto& [category, defaults] : feature_info->navigation.category_defaults) {
            QVariantMap parameters;
            for (const auto& [name, value] : defaults)
                parameters.insert(QString::fromStdString(name), QString::fromStdString(value));
            category_defaults.insert(QString::fromStdString(category), parameters);
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
                QString::fromStdString(feature_info->navigation.icon.empty() ? menu.icon : feature_info->navigation.icon),
                std::move(args),
                feature_info->interactive, nullptr, categories,
                QString::fromStdString(feature_info->navigation.group), feature_info->navigation.order,
                QString::fromStdString(feature_info->navigation.label), category_defaults));
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
