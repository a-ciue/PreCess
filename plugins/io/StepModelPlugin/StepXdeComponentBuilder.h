#pragma once
#include "ModelPayload.h"
#include <memory>
#include <optional>
#include <string>

class TDocStd_Document;

namespace systems::io {

class StepXdeComponentBuilder {
public:
    static std::optional<ModelPayload> buildModelData(
        TDocStd_Document& doc,
        const std::string& modelName);
};
}