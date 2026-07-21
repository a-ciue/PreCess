#include "QGeometryOperations.h"

#include "ComponentData.h"
#include "ComponentOperator.h"
#include "GeometryBuilder.h"
#include "GeometryData.h"
#include "ModelLayer.h"
#include "QSelection.h"

#include <Standard_Failure.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Edge.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Vertex.hxx>
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
// 界面对用户统一使用度数，进入不依赖 Qt 的 GeometryBuilder 前转换为 OCC 使用的弧度。
double degreesToRadians(double angle_degrees)
{
    return angle_degrees * std::acos(-1.0) / 180.0;
}
}

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
    } catch (const std::exception& error) {
        spdlog::error("QGeometryOperations::createPoint: {}", error.what());
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
    } catch (const std::exception& error) {
        spdlog::error("QGeometryOperations::createLineByCoordinates: {}", error.what());
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
    } catch (const std::exception& error) {
        spdlog::error("QGeometryOperations::createLineFromVertices: {}", error.what());
    }
    return -1;
}

int QGeometryOperations::createRectangleFace(
    int modelId,
    int componentId,
    double originX,
    double originY,
    double originZ,
    double width,
    double height,
    int plane)
{
    try {
        TopoDS_Shape shape = GeometryBuilder::makeRectangleFace(
            originX,
            originY,
            originZ,
            width,
            height,
            static_cast<CoordinatePlane>(plane));
        const std::string component_name =
            "RectangleFace_" + std::to_string(next_rectangle_face_number_);
        const Index result_component_id = addGeometryShape(
            modelId, componentId, component_name, std::move(shape));
        if (componentId < 0)
            ++next_rectangle_face_number_;
        return result_component_id;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("QGeometryOperations::createRectangleFace: {}",
            detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("QGeometryOperations::createRectangleFace: {}", error.what());
    }
    return -1;
}

int QGeometryOperations::createDiskFace(
    int modelId,
    int componentId,
    double centerX,
    double centerY,
    double centerZ,
    double radius,
    int plane,
    double startAngle,
    double sweepAngle)
{
    try {
        TopoDS_Shape shape = GeometryBuilder::makeDiskFace(
            centerX,
            centerY,
            centerZ,
            radius,
            static_cast<CoordinatePlane>(plane),
            degreesToRadians(startAngle),
            degreesToRadians(sweepAngle));
        const std::string component_name =
            "DiskFace_" + std::to_string(next_disk_face_number_);
        const Index result_component_id = addGeometryShape(
            modelId, componentId, component_name, std::move(shape));
        if (componentId < 0)
            ++next_disk_face_number_;
        return result_component_id;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("QGeometryOperations::createDiskFace: {}",
            detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("QGeometryOperations::createDiskFace: {}", error.what());
    }
    return -1;
}

int QGeometryOperations::createFaceFromEdges(
    int componentId, QSelection* selection)
{
    try {
        if (componentId < 0)
            throw std::invalid_argument("Select a target component first");
        if (!selection || !selection->get())
            throw std::invalid_argument("Select geometry edges");

        const std::shared_ptr<Selection> selected = selection->get();
        if (selected->type != ElementEnum::GeometryEdge || selected->ids.empty())
            throw std::invalid_argument("At least one geometry edge is required");

        ComponentData* component = model_layer_->findComponent(componentId);
        if (!component || !component->geometry || !component->geometry->rootShape)
            throw std::invalid_argument("Target component has no geometry");
        component->geometry->ensureIndexBuilt(model_layer_->geomRegistry());

        const auto& component_edge_ids = component->geometry->index.edge_local_to_global;
        std::vector<TopoDS_Edge> edges;
        edges.reserve(selected->ids.size());
        for (Index id : selected->ids) {
            if (std::find(component_edge_ids.begin(), component_edge_ids.end(), id)
                == component_edge_ids.end())
                throw std::invalid_argument("Selected edges must belong to the target component");

            const TopoDS_Shape* edge_shape = model_layer_->geomRegistry().getEdge(id);
            if (!edge_shape || edge_shape->ShapeType() != TopAbs_EDGE)
                throw std::invalid_argument("Selected geometry edges are no longer valid");
            // 追加 Face 会重建索引，因此先复制轻量 TopoDS_Edge 句柄。
            edges.push_back(TopoDS::Edge(*edge_shape));
        }

        TopoDS_Shape face = GeometryBuilder::makeFaceFromEdges(edges);
        return addGeometryShape(-1, componentId, "FaceFromEdges", std::move(face));
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("QGeometryOperations::createFaceFromEdges: {}",
            detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("QGeometryOperations::createFaceFromEdges: {}", error.what());
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
    } catch (const std::exception& error) {
        spdlog::error("QGeometryOperations::createBox: {}", error.what());
    }
    return -1;
}

int QGeometryOperations::createCylinder(
    int modelId,
    int componentId,
    double centerX,
    double centerY,
    double centerZ,
    double radius,
    double height,
    double directionX,
    double directionY,
    double directionZ,
    double sweepAngle)
{
    try {
        TopoDS_Shape shape = GeometryBuilder::makeCylinder(
            centerX,
            centerY,
            centerZ,
            radius,
            height,
            directionX,
            directionY,
            directionZ,
            degreesToRadians(sweepAngle));
        const std::string component_name =
            "Cylinder_" + std::to_string(next_cylinder_number_);
        const Index result_component_id = addGeometryShape(
            modelId, componentId, component_name, std::move(shape));
        if (componentId < 0)
            ++next_cylinder_number_;
        return result_component_id;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("QGeometryOperations::createCylinder: {}",
            detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("QGeometryOperations::createCylinder: {}", error.what());
    }
    return -1;
}

int QGeometryOperations::createCone(
    int modelId,
    int componentId,
    double centerX,
    double centerY,
    double centerZ,
    double bottomRadius,
    double topRadius,
    double height,
    double directionX,
    double directionY,
    double directionZ,
    double sweepAngle)
{
    try {
        TopoDS_Shape shape = GeometryBuilder::makeCone(
            centerX,
            centerY,
            centerZ,
            bottomRadius,
            topRadius,
            height,
            directionX,
            directionY,
            directionZ,
            degreesToRadians(sweepAngle));
        const std::string component_name =
            "Cone_" + std::to_string(next_cone_number_);
        const Index result_component_id = addGeometryShape(
            modelId, componentId, component_name, std::move(shape));
        if (componentId < 0)
            ++next_cone_number_;
        return result_component_id;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("QGeometryOperations::createCone: {}",
            detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("QGeometryOperations::createCone: {}", error.what());
    }
    return -1;
}

int QGeometryOperations::createSphere(
    int modelId,
    int componentId,
    double centerX,
    double centerY,
    double centerZ,
    double radius,
    double directionX,
    double directionY,
    double directionZ,
    double minimumLatitude,
    double maximumLatitude,
    double longitudeSweep)
{
    try {
        TopoDS_Shape shape = GeometryBuilder::makeSphere(
            centerX,
            centerY,
            centerZ,
            radius,
            directionX,
            directionY,
            directionZ,
            degreesToRadians(minimumLatitude),
            degreesToRadians(maximumLatitude),
            degreesToRadians(longitudeSweep));
        const std::string component_name =
            "Sphere_" + std::to_string(next_sphere_number_);
        const Index result_component_id = addGeometryShape(
            modelId, componentId, component_name, std::move(shape));
        if (componentId < 0)
            ++next_sphere_number_;
        return result_component_id;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("QGeometryOperations::createSphere: {}",
            detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("QGeometryOperations::createSphere: {}", error.what());
    }
    return -1;
}

int QGeometryOperations::extrudeFace(
    int componentId,
    QSelection* selection,
    double directionX,
    double directionY,
    double directionZ,
    double length)
{
    try {
        if (componentId < 0)
            throw std::invalid_argument("Select a target component first");
        if (!selection || !selection->get())
            throw std::invalid_argument("Select one geometry face");

        const std::shared_ptr<Selection> selected = selection->get();
        if (selected->type != ElementEnum::GeometryFace || selected->ids.size() != 1)
            throw std::invalid_argument("Exactly one geometry face is required");

        ComponentData* component = model_layer_->findComponent(componentId);
        if (!component || !component->geometry || !component->geometry->rootShape)
            throw std::invalid_argument("Target component has no geometry");
        component->geometry->ensureIndexBuilt(model_layer_->geomRegistry());

        const Index face_id = selected->ids.front();
        const auto& component_face_ids = component->geometry->index.face_local_to_global;
        if (std::find(component_face_ids.begin(), component_face_ids.end(), face_id)
            == component_face_ids.end())
            throw std::invalid_argument("Selected face must belong to the target component");

        const TopoDS_Shape* source_shape = model_layer_->geomRegistry().getFace(face_id);
        if (!source_shape || source_shape->ShapeType() != TopAbs_FACE)
            throw std::invalid_argument("Selected geometry face is no longer valid");

        // 在追加结果导致索引重建前复制句柄，构造器内部再复制源 Face 的拓扑。
        const TopoDS_Face source = TopoDS::Face(*source_shape);
        TopoDS_Shape solid = GeometryBuilder::extrudeFace(
            source, directionX, directionY, directionZ, length);
        const Index result_component_id = addGeometryShape(
            -1, componentId, "Extrude", std::move(solid));
        return result_component_id;
    } catch (const Standard_Failure& error) {
        const char* detail = error.GetMessageString();
        spdlog::error("QGeometryOperations::extrudeFace: {}",
            detail ? detail : "OpenCASCADE error");
    } catch (const std::exception& error) {
        spdlog::error("QGeometryOperations::extrudeFace: {}", error.what());
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
    if (component_id >= 0) {
        auto component_operator = model_layer_->getComponentOperator(component_id);
        if (!component_operator)
            throw std::invalid_argument("Target component does not exist");
        return component_operator->appendGeometryShape(std::move(shape));
    }

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
