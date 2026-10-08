#include "QAlgorithmSystemAdaptor.h"
#include "AlgorithmSystem.h"
#include "Job.h"
#include "QAlgorithmInfo.h"
#include "QArgObject.h"
#include "QTaskStatus.h"
#include <spdlog/spdlog.h>
#include <stdexcept>

namespace systems::algo {
QAlgorithmSystemAdaptor::QAlgorithmSystemAdaptor(AlgorithmSystem& algo_system,
    QTaskStatus& task_status)
    : algo_system_(&algo_system)
    , task_status_(&task_status)
{
    algo_system.setOnAlgorithmInfosChanged([this]() {
        emit algorithmsInfoChanged();
    });
}
QAlgorithmSystemAdaptor::~QAlgorithmSystemAdaptor() = default;

void QAlgorithmSystemAdaptor::call(const QString& unique_name, Index model, const QVariantList& args)
{
    std::optional arg_types = algo_system_->getArgTypes(unique_name.toStdString());
    if (!arg_types) {
        spdlog::error("AlgorithmSystemAdaptor::call: Algorithm {} not found", unique_name.toStdString());
        return;
    }
    if (args.size() < arg_types->size()) {
        spdlog::error("AlgorithmSystemAdaptor::call: Algorithm {} requires {} arguments, but {} were provided",
            unique_name.toStdString(), arg_types->size(), args.size());
        return;
    }

    // 转换到C++标准库类型，并检验所需类型
    std::vector<core::ArgObject> converted_args;
    converted_args.reserve(arg_types->size());
    for (size_t i = 0; i < arg_types->size(); i++) {
        const core::ArgType& type = (*arg_types)[i];
        QArgType q_type(type);
        QArgObject q_object(q_type);
        q_object.setValue(args[i]);

        if (std::optional value = q_object.getValue()) {
            converted_args.push_back(*value);
        } else {
            spdlog::error("AlgorithmSystemAdaptor::call: Argument {} not valid", type.name);
            return;
        }
    }

    try {
        if (!algo_system_->callAsync(unique_name.toStdString(), model, std::move(converted_args)))
            spdlog::warn("AlgorithmSystemAdaptor: job rejected while another operation is pending");
    } catch (const std::exception& e) {
        spdlog::error("AlgorithmSystemAdaptor: {}", e.what());
        task_status_->reportFailure(QString::fromStdString(e.what()));
    }
}

QList<QAlgorithmInfo*> QAlgorithmSystemAdaptor::getAlgorithmsInfo() const
{
    QList<QAlgorithmInfo*> infos;
    for (const auto& algo_info : algo_system_->getAlgorithmInfos()) {
        QList<QArgType*> args;
        for (const auto& arg_type : algo_info->arg_types) {
            args << new QArgType(arg_type);
        }
        QStringList categories;
        for (const auto& category : algo_info->navigation.categories)
            categories.append(QString::fromStdString(category));
        QVariantMap category_defaults;
        for (const auto& [category, defaults] : algo_info->navigation.category_defaults) {
            QVariantMap parameters;
            for (const auto& [name, value] : defaults)
                parameters.insert(QString::fromStdString(name), QString::fromStdString(value));
            category_defaults.insert(QString::fromStdString(category), parameters);
        }
        infos.append(new QAlgorithmInfo(
            QString::fromStdString(algo_info->name),
            QString::fromStdString(algo_info->display_name),
            QString::fromStdString(algo_info->description),
            std::move(args), nullptr, std::move(categories),
            QString::fromStdString(algo_info->navigation.group),
            QString::fromStdString(algo_info->navigation.icon),
            algo_info->navigation.order,
            QString::fromStdString(algo_info->navigation.label), std::move(category_defaults)));
    }
    return infos;
}

}
