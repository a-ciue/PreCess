/**
 * @file IgesXdeComponentBuilder.h
 * @brief IGES XDE 组件构建器
 * @author 范成通
 */
#pragma once
#include <memory>
#include <string>

class ModelData;
class TDocStd_Document;

namespace systems::io {

class IgesXdeComponentBuilder {
public:
    static std::unique_ptr<ModelData> buildModelData(
        TDocStd_Document& doc,
        const std::string& modelName);
};

}