#pragma once
#include "IAttributeRenderStrategy.h"
#include "AttributeOperator.h"
class AttriRenderStrategyVector : public IAttributeRenderStrategy {
public:
    void Render(
        AttributeOperator* op,
        const std::string& attr_name,
        std::map<std::string, std::any> args) override;

private:
    void createGlyph3D(AttributeOperator* op, vtkDataSet* input, const std::array<double, 3>& color, double scale = 0.3);
};