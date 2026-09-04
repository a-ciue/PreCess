#include "QModelIOSystemAdaptor.h"
#include "ModelIOInfo.h"
#include "ModelIOSystem.h"
#include "QArgObject.h"
#include "QModelIOInfo.h"
#include <QFileInfo>
#include <QUrl>
#include <spdlog/fmt/ranges.h>
#include <spdlog/spdlog.h>

using namespace systems::io;

QModelIOSystemAdaptor::QModelIOSystemAdaptor(ModelIOSystem& io_system)
    : io_system_(&io_system)
{
    io_system.setOnDialogNameFiltersChanged([this]() {
        emit dialogNameFiltersChanged();
    });
}

bool QModelIOSystemAdaptor::read(const QString& unique_name, const QUrl& url, const QVariantList& args)
try {
    // 转换到C++标准库类型，并检验所需类型
    std::vector<std::any> converted_args;
    // TODO: 暂时不对args处理

    std::string resolved_unique_name = unique_name.toStdString();
    const auto all_infos = io_system_->registeredFileTypeInfos();

    auto is_registered_name = [&](const std::string& name) {
        return std::any_of(all_infos.begin(), all_infos.end(), [&](const ModelIOInfo* info) {
            return info->name == name;
        });
    };

    // 未指定或指定了未知类型时，回退按扩展名解析文件类型
    if (resolved_unique_name == "All files" || !is_registered_name(resolved_unique_name)) {
        const QString resolved_by_suffix = resolveFileTypeBySuffix(QFileInfo(url.toLocalFile()).suffix().toLower());
        if (!resolved_by_suffix.isEmpty()) {
            resolved_unique_name = resolved_by_suffix.toStdString();
        }
    }

    if (!is_registered_name(resolved_unique_name)) {
        spdlog::error("ModelIOSystemAdaptor::read: file type '{}' not registered", unique_name.toStdString());
        return false;
    }

    // 透传读取结果：文件类型未注册或文件内容无法解析时返回false，供界面收集失败文件
    return io_system_->read(url.toLocalFile().toLocal8Bit().toStdString(), resolved_unique_name, converted_args);
} catch (const std::exception& e) {
    spdlog::error("ModelIOSystemAdaptor::read: Exception occurred - {}", e.what());
    return {};
} catch (...) {
    spdlog::error("ModelIOSystemAdaptor::read: Unknown exception occurred");
    return {};
}

bool QModelIOSystemAdaptor::write(const QString& unique_name, Index model, const QUrl& url, const QVariantList& args)
try {
    // 将QArgObject列表转换为std::vector<std::any>
    std::vector<std::any> any_args;
    // TODO: 暂时不对args处理
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
        QList<QArgType*> read_args;
        for (const auto& arg_type : file_type_info->read_arg_types) {
            read_args << new QArgType(arg_type);
        }
        QList<QArgType*> write_args;
        for (const auto& arg_type : file_type_info->write_arg_types) {
            write_args << new QArgType(arg_type);
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

QString QModelIOSystemAdaptor::resolveFileTypeBySuffix(const QString& suffix) const
{
    // io_system_ 可能尚未就绪，防御性判空
    if (!io_system_) {
        return {};
    }
    for (const ModelIOInfo* info : io_system_->registeredFileTypeInfos()) {
        for (const auto& extension : info->extensions) {
            if (suffix.compare(QString::fromStdString(extension), Qt::CaseInsensitive) == 0) {
                return QString::fromStdString(info->name);
            }
        }
    }
    return {};
}

QStringList QModelIOSystemAdaptor::getDialogNameFilters() const
try {
    QStringList filters;
    // 添加"所有文件"选项，方便用户选择任意文件
    filters << "All files (*)";

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