#include "QGeometryOperations.h"

#include "ComponentData.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "ModelLayer.h"
#include "QSelection.h"

#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <algorithm>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>

QGeometryOperations::QGeometryOperations(ModelLayer& model_layer, QObject* parent)
    : QObject(parent)
    , model_layer_(&model_layer)
{
}

int QGeometryOperations::createPoint(
    int modelId, int componentId, double x, double y, double z)
{
    try {
        TopoDS_Shape shape = GeometryBuilder::makePoint(x, y, z);
        const std::string component_name = "Point_" + std::to_string(next_point_number_);
        const Index result_component_id = addGeometryShape(
            modelId, componentId, component_name, std::move(shape));
        if (componentId < 0)
            ++next_point_number_;
        return result_component_id;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("QGeometryOperations::createPoint: {}",
            detail ? detail : "OpenCASCADE error");
        emit operationFailed(
            QStringLiteral("创建 Point 失败：%1")
                .arg(QString::fromLocal8Bit(detail ? detail : "OpenCASCADE error")));
    } catch (const std::exception& error) {
        spdlog::error("QGeometryOperations::createPoint: {}", error.what());
        emit operationFailed(
            QStringLiteral("创建 Point 失败：%1").arg(QString::fromLocal8Bit(error.what())));
    }
    return -1;
}

int QGeometryOperations::createLineByCoordinates(
    int modelId,
    int componentId,
    double startX,
    double startY,
    double startZ,
    double endX,
    double endY,
    double endZ)
{
    try {
        TopoDS_Shape shape = GeometryBuilder::makeLine(
            startX, startY, startZ, endX, endY, endZ);
        const std::string component_name = "Line_" + std::to_string(next_line_number_);
        const Index result_component_id = addGeometryShape(
            modelId, componentId, component_name, std::move(shape));
        if (componentId < 0)
            ++next_line_number_;
        return result_component_id;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("QGeometryOperations::createLineByCoordinates: {}",
            detail ? detail : "OpenCASCADE error");
        emit operationFailed(
            QStringLiteral("创建 Line 失败：%1")
                .arg(QString::fromLocal8Bit(detail ? detail : "OpenCASCADE error")));
    } catch (const std::exception& error) {
        spdlog::error("QGeometryOperations::createLineByCoordinates: {}", error.what());
        emit operationFailed(
            QStringLiteral("创建 Line 失败：%1").arg(QString::fromLocal8Bit(error.what())));
    }
    return -1;
}

int QGeometryOperations::createLineFromVertices(
    int componentId, QSelection* selection)
{
    try {
        if (componentId < 0)
            throw std::invalid_argument("Select a target component first");
        if (!selection || !selection->get())
            throw std::invalid_argument("Select two geometry vertices");

        const std::shared_ptr<Selection> selected = selection->get();
        if (selected->type != ElementEnum::GeometryVertex || selected->ids.size() != 2)
            throw std::invalid_argument("Exactly two geometry vertices are required");

        ComponentData* component = model_layer_->findComponent(componentId);
        if (!component || !component->geometry || !component->geometry->rootShape)
            throw std::invalid_argument("Target component has no geometry");
        component->geometry->ensureIndexBuilt(model_layer_->geomRegistry());

        const auto& component_vertex_ids = component->geometry->index.vertex_local_to_global;
        for (Index id : selected->ids) {
            if (std::find(component_vertex_ids.begin(), component_vertex_ids.end(), id)
                == component_vertex_ids.end())
                throw std::invalid_argument("Selected vertices must belong to the target component");
        }

        const TopoDS_Shape* start_shape = model_layer_->geomRegistry().getVertex(selected->ids[0]);
        const TopoDS_Shape* end_shape = model_layer_->geomRegistry().getVertex(selected->ids[1]);
        if (!start_shape || !end_shape
            || start_shape->ShapeType() != TopAbs_VERTEX
            || end_shape->ShapeType() != TopAbs_VERTEX)
            throw std::invalid_argument("Selected geometry vertices are no longer valid");

        // 在索引释放前复制 TopoDS_Vertex，随后用它们构造共享端点的 Edge。
        const TopoDS_Vertex start = TopoDS::Vertex(*start_shape);
        const TopoDS_Vertex end = TopoDS::Vertex(*end_shape);
        TopoDS_Shape line = GeometryBuilder::makeLine(start, end);
        return addGeometryShape(
            -1, componentId, "Line_" + std::to_string(next_line_number_), std::move(line));
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("QGeometryOperations::createLineFromVertices: {}",
            detail ? detail : "OpenCASCADE error");
        emit operationFailed(
            QStringLiteral("创建 Line 失败：%1")
                .arg(QString::fromLocal8Bit(detail ? detail : "OpenCASCADE error")));
    } catch (const std::exception& error) {
        spdlog::error("QGeometryOperations::createLineFromVertices: {}", error.what());
        emit operationFailed(
            QStringLiteral("创建 Line 失败：%1").arg(QString::fromLocal8Bit(error.what())));
    }
    return -1;
}

int QGeometryOperations::createBox(
    int modelId,
    int componentId,
    double originX,
    double originY,
    double originZ,
    double lengthX,
    double lengthY,
    double lengthZ)
{
    try {
        TopoDS_Shape shape = GeometryBuilder::makeBox(
            originX, originY, originZ, lengthX, lengthY, lengthZ);
        const std::string component_name = "Box_" + std::to_string(next_box_number_);
        const Index result_component_id = addGeometryShape(
            modelId, componentId, component_name, std::move(shape));
        if (componentId < 0)
            ++next_box_number_;
        return result_component_id;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("QGeometryOperations::createBox: {}",
            detail ? detail : "OpenCASCADE error");
        emit operationFailed(
            QStringLiteral("创建 Box 失败：%1")
                .arg(QString::fromLocal8Bit(detail ? detail : "OpenCASCADE error")));
    } catch (const std::exception& error) {
        spdlog::error("QGeometryOperations::createBox: {}", error.what());
        emit operationFailed(
            QStringLiteral("创建 Box 失败：%1").arg(QString::fromLocal8Bit(error.what())));
    }
    return -1;
}

Index QGeometryOperations::addGeometryShape(
    Index model_id,
    Index component_id,
    std::string component_name,
    TopoDS_Shape shape)
{
    // Component 目标优先：将新形状追加到其现有 Geometry。
    if (component_id >= 0)
        return model_layer_->appendGeometryShape(component_id, std::move(shape));

    const std::string model_name = "temp_" + component_name;

    auto geometry = std::make_unique<GeometryData>();
    geometry->rootShape = std::make_unique<TopoDS_Shape>(std::move(shape));

    auto component = std::make_unique<ComponentData>();
    component->name = std::move(component_name);
    component->geometry = std::move(geometry);

    if (model_id >= 0) {
        if (!model_layer_->modelById(model_id))
            throw std::invalid_argument("Target model does not exist");
        return model_layer_->addGeometryComponent(model_id, std::move(component));
    }

    ComponentDatas components;
    components.push_back(std::move(component));
    const Index new_model_id = model_layer_->addModel(model_name, std::move(components));
    ModelData* model = model_layer_->modelById(new_model_id);
    if (!model || model->componentIds().empty())
        throw std::runtime_error("Failed to add the geometry component");
    return model->componentIds().back();
}
