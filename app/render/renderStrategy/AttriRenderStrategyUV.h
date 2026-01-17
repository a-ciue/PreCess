#pragma once
#include "IAttributeRenderStrategy.h"
#include "AttributeOperator.h"
class AttriRenderStrategyUV : public IAttributeRenderStrategy {
public:
    void render(
        AttributeOperator& op,
        const std::string& attr_name,
        std::map<std::string, std::any> args) override;
};