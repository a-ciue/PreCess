#include "QAlgorithmSystemAdaptor.h"
#include "AlgorithmSystem.h"
#include "QAlgorithmInfo.h"
#include "QArgObject.h"
#include <QVariant>
#include <spdlog/spdlog.h>

namespace systems::algo {
QAlgorithmSystemAdaptor::QAlgorithmSystemAdaptor(AlgorithmSystem& algo_system)
    : algo_system_(&algo_system)
{
}

QVariant QAlgorithmSystemAdaptor::call(const QString& unique_name, Index model, const QList<QArgObject*>& args)
{
    // 转换到C++标准库类型，并检验所需类型
    std::vector<std::any> converted_args;
    converted_args.reserve(args.size());
    for (QArgObject* arg : args) {
        if (std::optional value = arg->getValue()) {
            converted_args.push_back(*value);
        } else {
            spdlog::error("AlgorithmSystemAdaptor::call: Argument {} not valid", arg->name().toLocal8Bit().toStdString());
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
        QList<QArgObject*> args;
        for (const auto& arg_type : algo_info->arg_types) {
            args.append(new QArgObject(arg_type));
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
