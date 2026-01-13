#include "QAlgorithmSystemAdaptor.h"
#include "AlgorithmSystem.h"
#include "QAlgorithmInfo.h"
#include "QArgObject.h"
#include <spdlog/spdlog.h>

namespace systems::algo {
QAlgorithmSystemAdaptor::QAlgorithmSystemAdaptor(AlgorithmSystem& algo_system)
    : algo_system_(&algo_system)
{
    algo_system.setOnAlgorithmInfosChanged([this]() {
        emit algorithmsInfoChanged();
    });
}

QVariant QAlgorithmSystemAdaptor::call(const QString& unique_name, Index model, const QVariantList& args)
{
    std::optional arg_types = algo_system_->getArgTypes(unique_name.toStdString());
    if (!arg_types) {
        spdlog::error("AlgorithmSystemAdaptor::call: Algorithm {} not found", unique_name.toStdString());
        return {};
    }
    if (args.size() < arg_types->size()) {
        spdlog::error("AlgorithmSystemAdaptor::call: Algorithm {} requires {} arguments, but {} were provided",
            unique_name.toStdString(), arg_types->size(), args.size());
        return {};
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
            return {};
        }
    }

    auto result = algo_system_->call(unique_name.toStdString(), model, std::move(converted_args));
    return {};
}

QList<QAlgorithmInfo*> QAlgorithmSystemAdaptor::getAlgorithmsInfo() const
{
    QList<QAlgorithmInfo*> infos;
    for (const auto& algo_info : algo_system_->getAlgorithmInfos()) {
        QList<QArgType*> args;
        for (const auto& arg_type : algo_info->arg_types) {
            args << new QArgType(arg_type);
        }
        infos.append(new QAlgorithmInfo(
            QString::fromStdString(algo_info->name),
            QString::fromStdString(algo_info->display_name),
            QString::fromStdString(algo_info->description),
            std::move(args)));
    }
    return infos;
}
}
