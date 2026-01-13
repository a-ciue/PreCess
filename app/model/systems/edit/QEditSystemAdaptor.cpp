#include "QEditSystemAdaptor.h"
#include "ArgObject.h"
#include "EditSystem.h"
#include "QArgObject.h"
#include "QEditInfo.h"
#include <spdlog/spdlog.h>

namespace systems::edit {
using core::ArgObject;
QEditSystemAdaptor::QEditSystemAdaptor(EditSystem& edit_system)
    : edit_system_(&edit_system)
{
    edit_system.setOnEditInfoChangedCallback([this]() {
        emit editInfoChanged();
    });
}

QVariant QEditSystemAdaptor::call(const QString& unique_name, Index model, const QVariantList& args)
{
    std::optional arg_types = edit_system_->getArgTypes(unique_name.toStdString());
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
            spdlog::error("EditSystemAdaptor::call: Argument {} not valid", type.name);
            return {};
        }
    }

    auto result = edit_system_->call(unique_name.toStdString(), model, std::move(converted_args));
    return {};
}

QList<QEditInfo*> QEditSystemAdaptor::getEditsInfo() const
{
    QList<QEditInfo*> infos;
    for (const auto& edit_info : edit_system_->getEditInfos()) {
        QList<QArgType*> args;
        for (const auto& arg_type : edit_info->arg_types) {
            args.append(new QArgType(arg_type));
        }
        infos.append(new QEditInfo(
            QString::fromStdString(edit_info->name),
            QString::fromStdString(edit_info->display_name),
            QString::fromStdString(edit_info->description),
            std::move(args)));
    }
    return infos;
}
}
