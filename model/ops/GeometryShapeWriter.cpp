#include "GeometryShapeWriter.h"

#include "ComponentData.h"
#include "ComponentOperator.h"
#include "GeometryData.h"
#include "ModelData.h"
#include "ModelLayer.h"
#include "ModelOperator.h"

#include <stdexcept>
#include <utility>
#include <vector>

Index GeometryShapeWriter::writeShape(
    ModelLayer& model_layer,
    const WriteTarget& target,
    std::string component_name,
    TopoDS_Shape shape)
{
    // Component 目标优先：将新形状追加到其现有 Geometry（写入即标脏，通知由操作边界 flush）
    if (target.component_id >= 0) {
        auto component_operator = model_layer.getComponentOperator(target.component_id);
        if (!component_operator)
            throw std::invalid_argument("Target component does not exist");
        return component_operator->appendGeometryShape(std::move(shape));
    }

    const std::string model_name = "temp_" + component_name;

    auto geometry = std::make_unique<GeometryData>();
    geometry->setRootShape(std::move(shape));

    auto component = std::make_unique<ComponentData>();
    component->name = std::move(component_name);
    component->geometry = std::move(geometry);

    if (target.model_id >= 0) {
        auto model_operator = model_layer.getModelOperator(target.model_id);
        if (!model_operator)
            throw std::invalid_argument("Target model does not exist");
        return model_operator->addGeometryComponent(std::move(component));
    }

    ComponentDatas components;
    components.push_back(std::move(component));
    const Index new_model_id = model_layer.addModel(model_name, std::move(components));
    ModelData* model = model_layer.modelById(new_model_id);
    if (!model || model->componentIds().empty())
        throw std::runtime_error("Failed to add the geometry component");
    return model->componentIds().back();
}
