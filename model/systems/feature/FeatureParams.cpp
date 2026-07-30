/**
 * @file FeatureParams.cpp
 */
#include "FeatureParams.h"

#include <filesystem>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>

namespace systems::feature {
namespace {
    long long parseInt(const std::string& content)
    {
        try {
            return content.empty() ? 0LL : std::stoll(content);
        } catch (const std::exception&) {
            return 0LL;
        }
    }

    double parseFloat(const std::string& content)
    {
        try {
            return content.empty() ? 0.0 : std::stod(content);
        } catch (const std::exception&) {
            return 0.0;
        }
    }

    // Combo 的 content 形如 "选项1,选项2|默认索引"，缺省索引为 0
    int parseComboIndex(const std::string& content)
    {
        const auto pos = content.find('|');
        if (pos == std::string::npos) {
            return 0;
        }
        try {
            return std::stoi(content.substr(pos + 1));
        } catch (const std::exception&) {
            return 0;
        }
    }

    // 从 ArgType::content 解析参数默认值，解析失败时回落到零值
    core::ArgObject makeDefaultValue(const core::ArgType& type)
    {
        switch (type.type) {
        case ArgTypeEnum::Int:
            return core::ArgObject::create<ArgTypeEnum::Int>(parseInt(type.content));
        case ArgTypeEnum::Float:
            return core::ArgObject::create<ArgTypeEnum::Float>(parseFloat(type.content));
        case ArgTypeEnum::Text:
            return core::ArgObject::create<ArgTypeEnum::Text>(type.content);
        case ArgTypeEnum::Bool:
            return core::ArgObject::create<ArgTypeEnum::Bool>(type.content == "true");
        case ArgTypeEnum::Path:
            return core::ArgObject::create<ArgTypeEnum::Path>(std::filesystem::path(type.content));
        case ArgTypeEnum::Combo:
            return core::ArgObject::create<ArgTypeEnum::Combo>(parseComboIndex(type.content));
        case ArgTypeEnum::Selector:
            return core::ArgObject::create<ArgTypeEnum::Selector>({});
        case ArgTypeEnum::Button:
            // Button 为无值触发器：默认 0 次点击，载荷仅作触发事件的计数器
            return core::ArgObject::create<ArgTypeEnum::Button>(0);
        case ArgTypeEnum::None:
        default:
            spdlog::warn("FeatureParams: param '{}' has None type, storing empty value", type.name);
            return {};
        }
    }
}

FeatureParams::FeatureParams(std::vector<core::ArgType> types)
    : types_(std::move(types))
{
    values_.reserve(types_.size());
    for (const auto& type : types_) {
        values_.push_back(makeDefaultValue(type));
    }
}

const core::ArgObject& FeatureParams::value(std::size_t index) const
{
    if (index >= values_.size()) {
        throw std::out_of_range("FeatureParams::value: index out of range");
    }
    return values_[index];
}

void FeatureParams::setValue(std::size_t index, core::ArgObject new_value)
{
    if (index >= values_.size()) {
        throw std::out_of_range("FeatureParams::setValue: index out of range");
    }
    values_[index] = std::move(new_value);
}
}
