/**
 * @file FeatureIOSystemRegister.cpp
 * @brief 旧 IO 编解码与 Feature 参数、执行入口之间的桥接。
 */
#include "FeatureIOSystemRegister.h"
#include "FeatureContext.h"
#include "FeatureHandler.h"
#include "FeatureParams.h"
#include "FeatureRegistrar.h"
#include "FeatureSystem.h"
#include "JobRunner.h"
#include "ModelIOSystem.h"
#include "ModelIOSystemRegister.h"
#include "ModelLayer.h"
#include "Selection.h"

#include <algorithm>
#include <any>
#include <cassert>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace systems::feature {
namespace {
    std::string fileFeatureName(FileDirection direction, const std::string& key)
    {
        return (direction == FileDirection::Import ? "LegacyIO.Import/" : "LegacyIO.Export/") + key;
    }

    template <ArgTypeEnum Type>
    std::any copyArgument(const core::ArgObject& value)
    {
        const auto* argument = value.get<Type>();
        assert(argument);
        return *argument;
    }

    std::any convertArgument(const core::ArgType& type, const core::ArgObject& value)
    {
        switch (type.type) {
        case ArgTypeEnum::Int:
            return copyArgument<ArgTypeEnum::Int>(value);
        case ArgTypeEnum::Float:
            return copyArgument<ArgTypeEnum::Float>(value);
        case ArgTypeEnum::Bool:
            return copyArgument<ArgTypeEnum::Bool>(value);
        case ArgTypeEnum::Text:
            return copyArgument<ArgTypeEnum::Text>(value);
        case ArgTypeEnum::Path:
            return copyArgument<ArgTypeEnum::Path>(value);
        case ArgTypeEnum::Paths:
            return copyArgument<ArgTypeEnum::Paths>(value);
        case ArgTypeEnum::Combo:
            return copyArgument<ArgTypeEnum::Combo>(value);
        case ArgTypeEnum::Selector:
            return copyArgument<ArgTypeEnum::Selector>(value);
        case ArgTypeEnum::Button:
            return copyArgument<ArgTypeEnum::Button>(value);
        default:
            return { };
        }
    }

}

class FeatureIOSystemRegister::FileFeature final : public FeatureHandler {
public:
    FileFeature(FeatureSystem& system, const io::ModelIOInfo& info, FileDirection direction)
        : system_(&system)
        , file_type_(info.name)
        , extensions_(info.extensions)
        , direction_(direction)
        , argument_types_(direction == FileDirection::Import ? info.read_arg_types : info.write_arg_types)
    {
    }

    void setup(FeatureRegistrar& registrar, FeatureContext&) override
    {
        registrar.setFileType({ direction_, extensions_ });
        registrar.addParameter({ direction_ == FileDirection::Import ? ArgTypeEnum::Paths : ArgTypeEnum::Path,
            direction_ == FileDirection::Import ? "输入文件" : "输出文件", "", "请选择文件路径" });
        if (direction_ == FileDirection::Export)
            registrar.addParameter({ ArgTypeEnum::Selector, "导出组件", "Component", "请选择要导出的一个或多个组件" });
        for (const auto& argument : argument_types_)
            registrar.addParameter(argument);
    }

    std::any execute(FeatureContext& context) override
    {
        const auto* path = context.params.value(0).get<ArgTypeEnum::Path>();
        const auto* paths = context.params.value(0).get<ArgTypeEnum::Paths>();
        if (direction_ == FileDirection::Export ? !path || path->empty() : !paths || paths->empty())
            return std::string("请选择文件路径。");
        std::vector<std::any> arguments;
        const std::size_t argument_offset = direction_ == FileDirection::Import ? 1 : 2;
        for (std::size_t index = 0; index < argument_types_.size(); ++index)
            arguments.push_back(convertArgument(argument_types_[index], context.params.value(index + argument_offset)));
        auto* io_system = system_->ioSystem();
        auto* runner = system_->job_runner_;
#ifdef __EMSCRIPTEN__
        // 网页保留同步 IO，上传文件和下载仍沿用当前生命周期。
        runner = nullptr;
#endif
        if (direction_ == FileDirection::Export) {
            const auto* selected = context.params.value(1).get<ArgTypeEnum::Selector>();
            if (!selected || !*selected || (*selected)->type != ElementEnum::Component || (*selected)->ids.empty())
                return std::string("请选择要导出的组件。");
            for (const auto component_id : (*selected)->ids) {
                if (!context.model.findComponent(component_id))
                    return std::string("导出组件已失效，请重新选择。");
            }
            if (runner) {
                // 占用期只读借用模型；writer 不得修改共享数据或缓存。
                return runner->run(file_type_, [io_system, ids = (*selected)->ids, output = *path, format = file_type_, arguments = std::move(arguments)] {
                    return job::JobWork { [=](job::ProgressFn report) {
                        report(0.0, "正在导出文件");
                        io_system->writeComponents(ids, output, format, arguments);
                        report(1.0, "导出处理结束");
                    } };
                },
                    { fileFeatureName(direction_, file_type_), true });
            }
            io_system->writeComponents((*selected)->ids, *path, file_type_, arguments);
            // 旧 writer 的正常返回尚不统一代表写入成功，适配层不新增成功承诺。
            return { };
        }
        if (runner) {
            return context.runWritebackJob(file_type_, [](const ModelLayer&) { return 0; }, [io_system, inputs = *paths, format = file_type_, arguments = std::move(arguments)](int&, job::ProgressFn report) {
                        std::vector<ModelPayload> models;
                        for (std::size_t index = 0; index < inputs.size(); ++index) {
                            report(static_cast<double>(index) / inputs.size(), pathUtf8(inputs[index]));
                            if (auto model = io_system->parseModel(inputs[index], format, arguments))
                                models.push_back(std::move(*model));
                        }
                        report(1.0, "文件解析结束");
                        return models; }, [](ModelLayer& layer, std::vector<ModelPayload>& models, job::ProgressFn) {
                        for (auto& model : models)
                            layer.addModel(u8Narrow(model.model_name), std::move(model.components)); }, true);
        }
        for (const auto& input : *paths)
            io_system->read(input, file_type_, arguments);
        return { };
    }

private:
    FeatureSystem* system_;
    std::string file_type_;
    std::vector<std::string> extensions_;
    FileDirection direction_;
    std::vector<core::ArgType> argument_types_;
};

FeatureIOSystemRegister::FeatureIOSystemRegister(FeatureSystem& system)
    : system_(&system)
{
    assert(system.ioSystem());
}

bool FeatureIOSystemRegister::registerPlugin(const QJsonObject& meta_data, PluginBase& plugin)
{
    system_->model_layer_->assertOperationIdle();
    const auto file_type = meta_data.value("file_type").toString().toStdString();
    if (file_type.empty())
        return false;
    // 适配器不能覆盖同名的已有功能。
    for (const auto direction : { FileDirection::Import, FileDirection::Export }) {
        if (system_->params(fileFeatureName(direction, file_type)))
            return false;
    }
    io::ModelIOSystemRegister registrar(*system_->ioSystem());
    if (!registrar.registerPlugin(meta_data, plugin))
        return false;
    const auto infos = system_->ioSystem()->registeredFileTypeInfos();
    const auto found = std::find_if(infos.begin(), infos.end(), [&](const auto* info) { return info->name == file_type; });
    assert(found != infos.end());
    const auto& info = **found;
    for (const auto direction : { FileDirection::Import, FileDirection::Export }) {
        HandlerMetaData feature;
        feature.name = fileFeatureName(direction, file_type);
        feature.display_name = file_type;
        feature.description = info.description;
        auto handler = std::make_unique<FileFeature>(*system_, info, direction);
        if (!system_->registerHandler(feature, FeatureSystem::SystemHandlerPtr { handler.release() })) {
            unregisterPlugin(meta_data);
            return false;
        }
    }
    return true;
}

void FeatureIOSystemRegister::unregisterPlugin(const QJsonObject& meta_data)
{
    // 忙时在移除任何条目前拒绝，保持两侧注册及插件列表不变。
    system_->model_layer_->assertOperationIdle();
    const auto file_type = meta_data.value("file_type").toString().toStdString();
    for (const auto direction : { FileDirection::Export, FileDirection::Import }) {
        HandlerMetaData feature;
        feature.name = fileFeatureName(direction, file_type);
        system_->unregisterHandler(feature);
    }
    io::ModelIOSystemRegister(*system_->ioSystem()).unregisterPlugin(meta_data);
}
}
