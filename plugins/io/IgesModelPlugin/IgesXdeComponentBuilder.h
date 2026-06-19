/**
 * @file IgesXdeComponentBuilder.h
 * @brief IGES XDE 组件构建器
 * @author 范成通
 */
#pragma once
#include "ModelPayload.h"
#include <memory>
#include <optional>
#include <string>

class TDocStd_Document;

namespace systems::io {

class IgesXdeComponentBuilder {
public:
    static std::optional<ModelPayload> buildModelData(
        TDocStd_Document& doc,
        const std::string& modelName);
};

}