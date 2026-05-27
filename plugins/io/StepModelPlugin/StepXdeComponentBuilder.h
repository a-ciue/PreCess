#pragma once
#include <memory>
#include <string>

class ModelData;
class TDocStd_Document;

namespace systems::io {

class StepXdeComponentBuilder {
public:
    static std::unique_ptr<ModelData> buildModelData(
        TDocStd_Document& doc,
        const std::string& modelName);
};
}