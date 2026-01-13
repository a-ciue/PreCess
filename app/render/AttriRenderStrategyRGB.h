#pragma once
#include "IAttributeRenderStrategy.h"
#include "AttributeOperator.h"
class AttriRenderStrategyRGB : public IAttributeRenderStrategy {
public:
    void Render(
        AttributeOperator* op,
        const std::string& attr_name,
        std::map<std::string, std::any> args) override;
};