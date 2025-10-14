#include "QModelIOSystemAdaptor.h"
#include "ModelIOInfo.h"
#include "ModelIOSystem.h"
#include "QArgObject.h"
#include "QModelIOInfo.h"
#include <QUrl>
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>

using namespace systems::io;

QModelIOSystemAdaptor::QModelIOSystemAdaptor(ModelIOSystem& io_system)
    : io_system_(&io_system)
{
}

bool QModelIOSystemAdaptor::read(const QString& unique_name, const QUrl& url, const QList<QArgObject*>& args)
try {
    // 转换到C++标准库类型，并检验所需类型
    std::vector<std::any> converted_args;
    converted_args.reserve(args.size());
    for (QArgObject* arg : args) {
        if (std::optional value = arg->getValue()) {
            converted_args.push_back(*value);
        } else {
            spdlog::error("AlgorithmSystemAdaptor::call: Argument {} not valid", arg->name().toStdString());
            return {};
        }
    }

    io_system_->read(url.toLocalFile().toLocal8Bit().toStdString(), unique_name.toStdString(), converted_args);
    return true;
} catch (const std::exception& e) {
    spdlog::error("ModelIOSystemAdaptor::read: Exception occurred - {}", e.what());
    return {};
} catch (...) {
    spdlog::error("ModelIOSystemAdaptor::read: Unknown exception occurred");
    return {};
}

bool QModelIOSystemAdaptor::write(const QString& unique_name, Index model, const QUrl& url, const QList<QArgObject*>& args)
try {
    // 将QArgObject列表转换为std::vector<std::any>
    std::vector<std::any> any_args;
    any_args.reserve(args.size());
    for (QArgObject* arg : args) {
        if (std::optional value = arg->getValue(); value) {
            any_args.push_back(*value);
        } else {
            spdlog::error("AlgorithmSystemAdaptor::call: Argument {} not valid", arg->name().toStdString());
            return {};
        }
    }
    io_system_->write(model, url.toLocalFile().toLocal8Bit().toStdString(), unique_name.toStdString(), std::move(any_args));
    return true;
} catch (const std::exception& e) {
    spdlog::error("ModelIOSystemAdaptor::write: Exception occurred - {}", e.what());
    return {};
} catch (...) {
    spdlog::error("ModelIOSystemAdaptor::write: Unknown exception occurred");
    return {};
}

QList<QModelIOInfo*> QModelIOSystemAdaptor::getModelIOInfo() const
try {
    QList<QModelIOInfo*> infos;
    for (ModelIOInfo* file_type_info : io_system_->registeredFileTypeInfos()) {
        QList<QArgObject*> read_args;
        for (const auto& arg_type : file_type_info->read_arg_types) {
            read_args << new QArgObject(arg_type);
        }
        QList<QArgObject*> write_args;
        for (const auto& arg_type : file_type_info->write_arg_types) {
            write_args << new QArgObject(arg_type);
        }

        infos << new QModelIOInfo(
            QString::fromStdString(file_type_info->name),
            QString::fromStdString(file_type_info->description),
            file_type_info->extensions,
            std::move(read_args),
            std::move(write_args));
    }
    return infos;
} catch (const std::exception& e) {
    spdlog::error("ModelIOSystemAdaptor::getModelIOInfo: Exception occurred - {}", e.what());
    return {};
} catch (...) {
    spdlog::error("ModelIOSystemAdaptor::getModelIOInfo: Unknown exception occurred");
    return {};
}

QStringList QModelIOSystemAdaptor::getDialogNameFilters() const
try {
    QStringList filters;
    for (ModelIOInfo* file_type_info : io_system_->registeredFileTypeInfos()) {
        filters << QString("%1 (*%2)")
                       .arg(QString::fromStdString(file_type_info->name))
                       .arg(file_type_info->extensions.empty()
                               ? ""
                               : QString::fromStdString(fmt::format(".{}", fmt::join(file_type_info->extensions, " *."))));
    }

    return filters;
} catch (const std::exception& e) {
    spdlog::error("ModelIOSystemAdaptor::getDialogNameFilters: Exception occurred - {}", e.what());
    return {};
} catch (...) {
    spdlog::error("ModelIOSystemAdaptor::getDialogNameFilters: Unknown exception occurred");
    return {};
}